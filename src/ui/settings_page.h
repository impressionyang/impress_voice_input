#pragma once

#include <QWidget>

class QFormLayout;
class QLineEdit;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QGroupBox;

namespace impress {

class ConfigManager;

/**
 * @brief 配置页面
 *
 * 管理模型路径、推理参数、音频设置等。
 */
class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(ConfigManager* configManager, QWidget* parent = nullptr);
    ~SettingsPage() override;

private slots:
    void onBrowseModelPath();
    void onBrowseTokensPath();
    void onSaveConfig();
    void onResetConfig();

private:
    void setupUI();
    void loadFromConfig();
    void saveToConfig();

    ConfigManager* configManager_;

    // STT 设置
    QLineEdit* modelPathEdit_;
    QPushButton* browseBtn_;
    QLineEdit* tokensPathEdit_;
    QPushButton* tokensBrowseBtn_;
    QComboBox* modelTypeCombo_;
    QComboBox* deviceCombo_;
    QSpinBox* threadSpin_;
    QSpinBox* sampleRateSpin_;
    QComboBox* languageCombo_;
    QCheckBox* streamingCheck_;
    QSpinBox* beamSizeSpin_;
    QDoubleSpinBox* temperatureSpin_;

    // 音频设置
    QSpinBox* bufferSizeSpin_;
    QSpinBox* chunkDurationSpin_;
    QSpinBox* paddingSpin_;

    // UI 设置
    QComboBox* themeCombo_;
    QSpinBox* fontSizeSpin_;
    QCheckBox* showWaveformCheck_;
    QCheckBox* showConfidenceCheck_;

    // 状态
    QLabel* statusLabel_;
};

} // namespace impress
