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
class HotkeyRecorder;

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
    void onBrowseAudioDebugDir();
    void onBrowseLogDir();
    void onSaveConfig();
    void onResetConfig();
    void onClearLogs();
    void onClearAudioFiles();

private:
    struct CleanupResult {
        int deletedCount;
        qint64 freedBytes;
    };

    void setupUI();
    void loadFromConfig();
    void saveToConfig();
    void populateAudioDevices();
    void selectAudioDevice(int deviceIndex);
    int getSelectedAudioDeviceIndex() const;
    CleanupResult clearDirectoryFiles(const QString& dirPath, const QStringList& filters, const QString& desc);

    ConfigManager* configManager_;

    // 通用设置
    QLineEdit* logDirEdit_;
    QPushButton* logDirBtn_;

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
    QCheckBox* debugSaveAudioCheck_;
    HotkeyRecorder* hotkeyRecorder_;
    QSpinBox* beamSizeSpin_;
    QDoubleSpinBox* temperatureSpin_;

    // 音频设置
    QComboBox* audioDeviceCombo_;
    QLineEdit* audioDebugDirEdit_;
    QPushButton* audioDebugDirBtn_;
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

    // 数据清理
    QPushButton* clearLogsBtn_;
    QPushButton* clearAudioBtn_;
};

} // namespace impress
