#pragma once

#include "McKernel.h"

#include <QSet>

// 第一期的占位内核：不碰 PrismLauncher，也不启动任何东西，只提供几个假实例把
// 界面与交互跑通。available() 恒为 false，界面据此提示"内核未接入"。
//
// 接 PrismKernel 时这个类原样保留 —— 它同时是内核层的回归测试替身。
class StubKernel : public McKernel
{
    Q_OBJECT

public:
    explicit StubKernel(QObject *parent = nullptr);

    QString name() const override;
    QString unavailableReason() const override;
    bool isRunning(const QString &instanceId) const override;

    void refresh() override;
    void launch(const QString &instanceId) override;
    void launchOffline(const QString &instanceId, const QString &playerName) override;

private:
    QSet<QString> m_running;
};
