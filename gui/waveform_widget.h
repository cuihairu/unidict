#pragma once

// 波形包络显示（发音练习 M1）：吃 downsample_wave 的输出画竖线包络，
// 纯展示组件，无音频设备依赖。
#include <QColor>
#include <QWidget>
#include <vector>

#include "pcm_util.h"

class WaveformWidget : public QWidget {
    Q_OBJECT
public:
    explicit WaveformWidget(QWidget* parent = nullptr);

public slots:
    void setWave(std::vector<PronAudio::WaveColumn> cols);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<PronAudio::WaveColumn> cols_;
    QColor waveColor_{0xb1, 0x19, 0x64};  // 项目 logo 主色，呼应品牌
};
