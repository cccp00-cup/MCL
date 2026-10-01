#include "Net.h"

void Net::configure(QNetworkRequest &request)
{
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    // 见头文件里的说明 —— 这一行是修「卡在查询版本不动」的关键
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    // Modrinth 要求带可识别的 User-Agent；其它源带上也无妨
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("mcl/0.1 (github.com/cccp00-cup/mcl)"));
}
