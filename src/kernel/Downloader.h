#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QThreadPool>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

// 一批下载任务里的一条
struct DownloadItem
{
    QString url;
    // 备用地址：镜像重试若干次仍失败时改用它（通常是官方源）。
    // 实测 BMCLAPI 会把请求 302 到各个镜像节点，个别节点上某些文件会持续 502，
    // 而重试还是落到同一个节点 —— 这时候只能换源。
    QString fallbackUrl;
    QString targetPath; // 绝对路径
    QString sha1;       // 空表示不校验
    qint64 size = -1;   // 已知大小，用于进度估算
    QString label;      // 出错时给人看的名字
};

// 并发下载器（校验与落盘都在线程池里做，不占用主线程）。
//
// 三条规矩：
//   1. 下载前先看本地文件在不在、大小和 SHA1 对不对 —— 对得上直接跳过，
//      所以第二次启动几乎只花校验的时间。
//   2. **边收边写 <target>.part**（不是攒在内存里等 finished），校验通过再改名 ——
//      既不会让 client.jar 这种大文件把内存顶爆，也让"在下"和"卡住"能区分开。
//   3. **所有 SHA1 计算和文件写入都丢进 QThreadPool** —— 这是之前最要命的地方：
//      在主线程同步算 SHA1，几十 MB 的库会让界面直接僵住。
//
// 每个任务都自带 generation 号：cancel / 重新 start 之后，旧任务的回调会被丢弃，
// 不会写坏新的那一批。
class Downloader : public QObject
{
    Q_OBJECT

public:
    explicit Downloader(QObject *parent = nullptr);
    ~Downloader() override;

    void start(const QList<DownloadItem> &items, int parallelism = 8);
    void cancel();

    bool isRunning() const { return m_running; }
    int total() const { return int(m_items.size()); }
    int done() const { return m_done; }

Q_SIGNALS:
    void progress(int done, int total, qint64 bytes, qint64 bytesTotal);
    void finished(bool ok, const QString &error);
    // 值得让用户看见的小状况（比如某个文件重试）
    void note(const QString &text);

private:
    friend class DownloadVerifyTask;
    friend class DownloadCommitTask;

    // 每条任务的状态：等待 → （校验已有文件）→ 下载 → 写入校验 → 完成
    enum class State { Pending, Busy, Done };

    void schedule();
    void checkStalls();
    void reportProgress();
    void fail(const QString &error);

    // 由线程池里的任务回调（都是队列连接，回到主线程执行）
    void taskVerified(quint64 generation, int index, bool upToDate);
    void taskCommitted(quint64 generation, int index, bool ok, const QString &error);

    QNetworkAccessManager *m_net = nullptr;
    QThreadPool m_pool;

    QList<DownloadItem> m_items;
    QList<State> m_state;
    QList<int> m_pending;   // 还没开始处理的索引，schedule 从队头取
    QList<int> m_retries;   // 每个文件已经重试过几次
    QList<qint64> m_received;
    int m_active = 0;
    int m_done = 0;
    int m_parallelism = 8;
    qint64 m_bytesTotal = 0;
    bool m_running = false;
    bool m_cancelled = false;
    quint64 m_generation = 0;

    QTimer *m_stallTimer = nullptr;
    QHash<QNetworkReply *, qint64> m_lastActivity; // 每个在途请求最后一次收到数据的时刻

    // 单个文件最多重试几次（网络抖一下就整批失败太脆弱了）
    static constexpr int kMaxRetries = 6;
    // 前面几次用镜像，超过这个次数就改用 fallbackUrl
    static constexpr int kFallbackAfterRetries = 1;
    // "没有新数据"多久算卡住。注意不能用 QNetworkRequest::setTransferTimeout ——
    // 那是**总时长**超时，会把 client.jar 这种大文件在慢链路上直接砍掉。
    // 这里只在意"数据还在不在流动"。
    static constexpr int kStallTimeoutMs = 45000;
    // 连大小都声明不出来的请求（Fabric 的 artifact 就没有 size 字段），
    // 给一个总时长上限兜底
    static constexpr qint64 kUnknownSizeTimeoutMs = 120000;
};
