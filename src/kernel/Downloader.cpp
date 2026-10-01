#include "Downloader.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QFileInfo>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRunnable>
#include <QThread>
#include <QTimer>
#include <QUrl>

namespace
{
// 线程安全：只依赖传入参数和文件系统，不碰 Downloader 的任何成员
bool verifyFile(const QString &path, const QString &sha1, qint64 size)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile())
        return false;
    if (size > 0 && info.size() != size)
        return false;
    if (sha1.isEmpty())
        return true;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QCryptographicHash hash(QCryptographicHash::Sha1);
    if (!hash.addData(&file))
        return false;
    return QString::fromLatin1(hash.result().toHex()).compare(sha1, Qt::CaseInsensitive) == 0;
}

// 校验已经落在盘上的 .part，通过了再改名。全部在 worker 线程里做。
// 内容已经在下载过程中流式写好了，这里不再碰内存。
bool commitPartFile(const DownloadItem &item, QString *error)
{
    const QString partPath = item.targetPath + QStringLiteral(".part");

    if (!item.sha1.isEmpty()) {
        QFile part(partPath);
        if (!part.open(QIODevice::ReadOnly)) {
            *error = QStringLiteral("读不到 %1").arg(partPath);
            return false;
        }
        QCryptographicHash hash(QCryptographicHash::Sha1);
        if (!hash.addData(&part)) {
            *error = QStringLiteral("校验 %1 失败").arg(partPath);
            return false;
        }
        const QString actual = QString::fromLatin1(hash.result().toHex());
        if (actual.compare(item.sha1, Qt::CaseInsensitive) != 0) {
            QFile::remove(partPath); // 坏的别留着，下次重下
            *error = QStringLiteral("SHA1 对不上");
            return false;
        }
    }

    QFile::remove(item.targetPath);
    if (!QFile::rename(partPath, item.targetPath)) {
        *error = QStringLiteral("改名失败：%1").arg(item.targetPath);
        return false;
    }
    return true;
}
} // namespace

// ——————————————————————————————————————————————— 线程池里的两个任务

// 校验一个已经存在的文件是不是已经是对的那一份
class DownloadVerifyTask : public QRunnable
{
public:
    DownloadVerifyTask(Downloader *owner, quint64 generation, int index, DownloadItem item)
        : m_owner(owner)
        , m_generation(generation)
        , m_index(index)
        , m_item(std::move(item))
    {
        setAutoDelete(true);
    }

    void run() override
    {
        // 任务自带数据副本 —— 就算 Downloader 那边已经 cancel 并清空了列表，
        // 这里也不会读到野数据
        const bool upToDate = verifyFile(m_item.targetPath, m_item.sha1, m_item.size);
        QMetaObject::invokeMethod(
            m_owner,
            [owner = m_owner, generation = m_generation, index = m_index, upToDate]() {
                owner->taskVerified(generation, index, upToDate);
            },
            Qt::QueuedConnection);
    }

private:
    Downloader *m_owner;
    quint64 m_generation;
    int m_index;
    DownloadItem m_item;
};

// 校验落盘的 .part 并改名
class DownloadCommitTask : public QRunnable
{
public:
    DownloadCommitTask(Downloader *owner, quint64 generation, int index, DownloadItem item)
        : m_owner(owner)
        , m_generation(generation)
        , m_index(index)
        , m_item(std::move(item))
    {
        setAutoDelete(true);
    }

    void run() override
    {
        QString error;
        const bool ok = commitPartFile(m_item, &error);
        QMetaObject::invokeMethod(
            m_owner,
            [owner = m_owner, generation = m_generation, index = m_index, ok, error]() {
                owner->taskCommitted(generation, index, ok, error);
            },
            Qt::QueuedConnection);
    }

private:
    Downloader *m_owner;
    quint64 m_generation;
    int m_index;
    DownloadItem m_item;
};

// ——————————————————————————————————————————————— Downloader

Downloader::Downloader(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
    // 校验和写盘都在这个池子里跑。上限取核数，留一个核给主线程渲染界面。
    m_pool.setMaxThreadCount(qMax(2, QThread::idealThreadCount()));

    m_stallTimer = new QTimer(this);
    m_stallTimer->setInterval(5000);
    connect(m_stallTimer, &QTimer::timeout, this, &Downloader::checkStalls);
    m_stallTimer->start();
}

// 只掐"不再有数据进来"的请求。看的是 downloadProgress 的活跃度，
// 所以慢但一直在传的大文件不会被误杀。
void Downloader::checkStalls()
{
    if (!m_running || m_cancelled || m_lastActivity.isEmpty())
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<QNetworkReply *> stalled;
    for (auto it = m_lastActivity.constBegin(); it != m_lastActivity.constEnd(); ++it) {
        if (now - it.value() > kStallTimeoutMs)
            stalled << it.key();
    }

    for (QNetworkReply *reply : stalled) {
        m_lastActivity.remove(reply);
        reply->abort(); // abort 会走到 finished，再由那边决定是重试还是失败
    }
}

Downloader::~Downloader()
{
    cancel();
}

void Downloader::cancel()
{
    ++m_generation; // 在途任务的回调全部失效
    m_cancelled = true;

    const QList<QNetworkReply *> replies = findChildren<QNetworkReply *>();
    for (QNetworkReply *reply : replies)
        reply->abort();

    m_pool.waitForDone(); // 等线程池里的校验/写盘收尾，之后再动成员才安全
    m_lastActivity.clear();

    m_running = false;
    m_active = 0;
    m_items.clear();
    m_state.clear();
    m_pending.clear();
    m_received.clear();
}

void Downloader::start(const QList<DownloadItem> &items, int parallelism)
{
    cancel();

    ++m_generation;
    m_items = items;
    m_state = QList<State>(items.size(), State::Pending);
    m_received = QList<qint64>(items.size(), 0);
    m_pending.clear();
    m_retries = QList<int>(items.size(), 0);
    for (int i = 0; i < items.size(); ++i)
        m_pending.append(i);

    m_active = 0;
    m_done = 0;
    m_parallelism = qMax(1, parallelism);
    m_bytesTotal = 0;
    m_cancelled = false;
    m_running = true;

    for (const DownloadItem &item : m_items) {
        if (item.size > 0)
            m_bytesTotal += item.size;
    }

    if (m_items.isEmpty()) {
        m_running = false;
        Q_EMIT finished(true, QString());
        return;
    }
    schedule();
}

void Downloader::schedule()
{
    if (m_cancelled || !m_running)
        return;

    while (m_active < m_parallelism && !m_pending.isEmpty()) {
        const int index = m_pending.takeFirst();
        m_state[index] = State::Busy;
        ++m_active;
        // 先校验本地已有的那份 —— 对得上就不用下载了
        m_pool.start(new DownloadVerifyTask(this, m_generation, index, m_items.at(index)));
    }

    reportProgress();

    if (m_done >= int(m_items.size()) && m_active == 0 && m_running) {
        m_running = false;
        Q_EMIT finished(true, QString());
    }
}

void Downloader::reportProgress()
{
    qint64 received = 0;
    for (qint64 value : m_received)
        received += value;
    Q_EMIT progress(m_done, int(m_items.size()), received, m_bytesTotal);
}

void Downloader::fail(const QString &error)
{
    ++m_generation;
    m_cancelled = true;

    const QList<QNetworkReply *> replies = findChildren<QNetworkReply *>();
    for (QNetworkReply *reply : replies)
        reply->abort();

    m_pool.waitForDone();
    m_lastActivity.clear();

    m_active = 0;
    m_running = false;
    Q_EMIT finished(false, error);
}

void Downloader::taskVerified(quint64 generation, int index, bool upToDate)
{
    if (generation != m_generation || m_cancelled || !m_running)
        return;
    if (index < 0 || index >= m_items.size())
        return;

    const DownloadItem item = m_items.at(index);

    if (upToDate) {
        m_received[index] = item.size > 0 ? item.size : 0;
        m_state[index] = State::Done;
        --m_active;
        ++m_done;
        schedule();
        return;
    }

    // 镜像重试若干次还在失败就换备用地址（一般就是官方源）
    const bool useFallback = m_retries.at(index) >= kFallbackAfterRetries
        && !item.fallbackUrl.isEmpty();
    const QString effectiveUrl = useFallback ? item.fallbackUrl : item.url;

    QNetworkRequest request{ QUrl(effectiveUrl) };
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mcl/0.1"));

    QNetworkReply *reply = m_net->get(request);
    // 交给停滞检测（checkStalls）来兜底，而不是总时长超时
    m_lastActivity.insert(reply, QDateTime::currentMSecsSinceEpoch());

    // 先把 .part 打开，收到多少写多少 —— 不攒内存，也能真实反映"正在下"
    QDir().mkpath(QFileInfo(item.targetPath).absolutePath());
    const QString partPath = item.targetPath + QStringLiteral(".part");
    auto *partFile = new QFile(partPath, reply);
    if (!partFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        reply->abort();
        fail(QStringLiteral("写不了 %1：%2").arg(partPath, partFile->errorString()));
        return;
    }

    const qint64 startedAt = QDateTime::currentMSecsSinceEpoch();
    connect(reply, &QNetworkReply::readyRead, this,
            [this, reply, partFile, index, startedAt]() {
                partFile->write(reply->readAll());

                // 大小守卫。某些镜像节点会**持续吐垃圾数据**：实测 fabric-loader 的 jar
                // 只有 2MB，却一路下到 16MB 还没停，而数据一直在流动，停滞检测抓不到。
                // 预期大小取「元数据里的 size」和「Content-Length」里较大的那个 ——
                // Fabric 的 artifact 根本没给 size，只能靠后者。
                const qint64 declared =
                    reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
                const qint64 expected = qMax(m_items.at(index).size, declared);
                const qint64 got = partFile->size();

                if (expected > 0) {
                    if (got > expected + expected / 2) {
                        Q_EMIT note(QStringLiteral("%1 收到的字节远超预期（%2 > %3），丢弃重来")
                                        .arg(m_items.at(index).label)
                                        .arg(got)
                                        .arg(expected));
                        reply->abort();
                    }
                } else if (QDateTime::currentMSecsSinceEpoch() - startedAt
                           > kUnknownSizeTimeoutMs) {
                    Q_EMIT note(QStringLiteral("%1 连大小都声明不出来且迟迟不完，重来一次")
                                    .arg(m_items.at(index).label));
                    reply->abort();
                }
            });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, reply, index](qint64 received, qint64) {
                // 有数据流动就刷新活跃时间
                m_lastActivity.insert(reply, QDateTime::currentMSecsSinceEpoch());
                m_received[index] = received;
                reportProgress();
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, generation, index, reply, effectiveUrl, partFile]() {
        m_lastActivity.remove(reply);
        reply->deleteLater();
        if (generation != m_generation || m_cancelled || !m_running)
            return;
        if (index < 0 || index >= m_items.size())
            return;

        if (reply->error() != QNetworkReply::NoError) {
            const DownloadItem &item = m_items.at(index);
            const QString what = item.label.isEmpty() ? effectiveUrl : item.label;

            // 网络抖一下不该让整批失败：重排队再来，超过次数才算真的失败
            if (m_retries.at(index) < kMaxRetries) {
                ++m_retries[index];
                --m_active;
                m_state[index] = State::Pending;
                m_pending.append(index);
                // 一个一个 arg()：混用双参重载会让后面的占位符对不上
                Q_EMIT note(QStringLiteral("%1 失败（%2），重试第 %3 次")
                                .arg(what)
                                .arg(reply->errorString())
                                .arg(m_retries.at(index)));
                schedule();
                return;
            }

            fail(QStringLiteral("下载失败：%1（%2）").arg(what, reply->errorString()));
            return;
        }

        // 把最后一批和缓冲里的都冲进文件，然后关掉
        if (partFile->isOpen()) {
            partFile->write(reply->readAll());
            partFile->flush();
            partFile->close();
        }
        const qint64 finalSize = m_items.at(index).size;
        m_received[index] = finalSize > 0 ? finalSize : m_received.at(index);
        reportProgress();
        // 校验 + 改名交给线程池
        m_pool.start(new DownloadCommitTask(this, generation, index, m_items.at(index)));
    });
}

void Downloader::taskCommitted(quint64 generation, int index, bool ok, const QString &error)
{
    if (generation != m_generation || m_cancelled || !m_running)
        return;
    if (index < 0 || index >= m_items.size())
        return;

    if (!ok) {
        fail(QStringLiteral("保存失败：%1（%2）").arg(m_items.at(index).label, error));
        return;
    }

    m_state[index] = State::Done;
    --m_active;
    ++m_done;
    schedule();
}
