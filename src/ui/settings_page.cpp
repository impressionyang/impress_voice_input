#include "settings_page.h"
#include "app/config_manager.h"
#include "audio/audio_capture.h"
#include "widgets/hotkey_recorder.h"
#include "utils/logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>

static const char* const kTag = "SettingsPage";

namespace impress {

SettingsPage::SettingsPage(ConfigManager* configManager, QWidget* parent)
    : QWidget(parent)
    , configManager_(configManager)
{
    setupUI();
    loadFromConfig();
}

SettingsPage::~SettingsPage() = default;

void SettingsPage::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // ---- 可滚动内容区域 ----
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* contentWidget = new QWidget(scrollArea);
    auto* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(12, 8, 12, 8);
    contentLayout->setSpacing(12);

    // 通用设置
    auto* appGroup = new QGroupBox("通用设置", contentWidget);
    auto* appLayout = new QFormLayout(appGroup);
    appLayout->setSpacing(8);
    appLayout->setContentsMargins(10, 12, 10, 12);

    auto* logDirRow = new QHBoxLayout();
    logDirEdit_ = new QLineEdit(this);
    logDirEdit_->setPlaceholderText("日志文件保存路径（为空时使用系统默认路径）");
    logDirBtn_ = new QPushButton("浏览...", this);
    connect(logDirBtn_, &QPushButton::clicked, this, &SettingsPage::onBrowseLogDir);
    logDirRow->addWidget(logDirEdit_);
    logDirRow->addWidget(logDirBtn_);
    appLayout->addRow("日志目录:", logDirRow);

    contentLayout->addWidget(appGroup);

    // STT 设置
    auto* sttGroup = new QGroupBox("STT 推理设置", contentWidget);
    auto* sttLayout = new QFormLayout(sttGroup);
    sttLayout->setSpacing(8);
    sttLayout->setContentsMargins(10, 12, 10, 12);

    auto* modelRow = new QHBoxLayout();
    modelPathEdit_ = new QLineEdit(this);
    modelPathEdit_->setPlaceholderText("选择 ONNX 模型文件路径...");
    browseBtn_ = new QPushButton("浏览...", this);
    connect(browseBtn_, &QPushButton::clicked, this, &SettingsPage::onBrowseModelPath);
    modelRow->addWidget(modelPathEdit_);
    modelRow->addWidget(browseBtn_);
    sttLayout->addRow("模型路径:", modelRow);

    modelTypeCombo_ = new QComboBox(this);
    modelTypeCombo_->addItems({"sense_voice", "whisper", "paraformer", "conformer"});
    sttLayout->addRow("模型类型:", modelTypeCombo_);

    deviceCombo_ = new QComboBox(this);
    deviceCombo_->addItems({"cpu", "gpu"});
    sttLayout->addRow("推理设备:", deviceCombo_);

    threadSpin_ = new QSpinBox(this);
    threadSpin_->setRange(1, 32);
    threadSpin_->setValue(4);
    sttLayout->addRow("推理线程数:", threadSpin_);

    auto* tokensRow = new QHBoxLayout();
    tokensPathEdit_ = new QLineEdit(this);
    tokensPathEdit_->setPlaceholderText("选择 tokens.txt 文件路径...");
    tokensBrowseBtn_ = new QPushButton("浏览...", this);
    connect(tokensBrowseBtn_, &QPushButton::clicked, this, &SettingsPage::onBrowseTokensPath);
    tokensRow->addWidget(tokensPathEdit_);
    tokensRow->addWidget(tokensBrowseBtn_);
    sttLayout->addRow("词表路径:", tokensRow);

    sampleRateSpin_ = new QSpinBox(this);
    sampleRateSpin_->setRange(8000, 192000);
    sampleRateSpin_->setSingleStep(1000);
    sampleRateSpin_->setValue(16000);
    sampleRateSpin_->setSuffix(" Hz");
    sttLayout->addRow("采样率:", sampleRateSpin_);

    languageCombo_ = new QComboBox(this);
    languageCombo_->addItems({"zh", "en", "ja", "ko", "fr", "de", "auto"});
    sttLayout->addRow("识别语言:", languageCombo_);

    streamingCheck_ = new QCheckBox("启用流式识别", this);
    streamingCheck_->setChecked(true);
    sttLayout->addRow("流式识别:", streamingCheck_);

    debugSaveAudioCheck_ = new QCheckBox("保存调试音频到临时文件夹", this);
    debugSaveAudioCheck_->setToolTip("开启后，每次识别会将原始音频保存为 WAV 文件到系统临时目录，用于调试音频质量问题");
    sttLayout->addRow("调试录音:", debugSaveAudioCheck_);

    // 快捷键录制按钮
    hotkeyRecorder_ = new HotkeyRecorder("语音快捷键:", this);
    hotkeyRecorder_->setToolTip("点击按钮后按下快捷键（如 Ctrl+Alt+K），支持组合键。Esc 取消录制。");
    connect(hotkeyRecorder_, &HotkeyRecorder::hotkeyChanged,
            this, [this](const QString& key) {
                configManager_->set("shortcuts.voice_hotkey", key);
            });
    sttLayout->addRow(hotkeyRecorder_);

    beamSizeSpin_ = new QSpinBox(this);
    beamSizeSpin_->setRange(1, 20);
    beamSizeSpin_->setValue(5);
    sttLayout->addRow("Beam Size:", beamSizeSpin_);

    temperatureSpin_ = new QDoubleSpinBox(this);
    temperatureSpin_->setRange(0.0, 2.0);
    temperatureSpin_->setSingleStep(0.1);
    temperatureSpin_->setValue(0.0);
    sttLayout->addRow("温度 (Temperature):", temperatureSpin_);

    contentLayout->addWidget(sttGroup);

    // 音频设置
    auto* audioGroup = new QGroupBox("音频设置", contentWidget);
    auto* audioLayout = new QFormLayout(audioGroup);
    audioLayout->setSpacing(8);
    audioLayout->setContentsMargins(10, 12, 10, 12);

    // 音频输入设备选择器
    audioDeviceCombo_ = new QComboBox(this);
    populateAudioDevices();
    audioLayout->addRow("输入设备:", audioDeviceCombo_);

    // 音频调试目录
    auto* debugDirRow = new QHBoxLayout();
    audioDebugDirEdit_ = new QLineEdit(this);
    audioDebugDirEdit_->setPlaceholderText("流式识别 WAV 文件保存路径（为空时使用系统临时目录）");
    audioDebugDirBtn_ = new QPushButton("浏览...", this);
    connect(audioDebugDirBtn_, &QPushButton::clicked, this, &SettingsPage::onBrowseAudioDebugDir);
    debugDirRow->addWidget(audioDebugDirEdit_);
    debugDirRow->addWidget(audioDebugDirBtn_);
    audioLayout->addRow("调试音频目录:", debugDirRow);

    bufferSizeSpin_ = new QSpinBox(this);
    bufferSizeSpin_->setRange(10, 100);
    bufferSizeSpin_->setValue(20);
    bufferSizeSpin_->setSuffix(" ms");
    audioLayout->addRow("缓冲区大小:", bufferSizeSpin_);

    chunkDurationSpin_ = new QSpinBox(this);
    chunkDurationSpin_->setRange(500, 10000);
    chunkDurationSpin_->setSingleStep(500);
    chunkDurationSpin_->setValue(3000);
    chunkDurationSpin_->setSuffix(" ms");
    audioLayout->addRow("推理块时长:", chunkDurationSpin_);

    paddingSpin_ = new QSpinBox(this);
    paddingSpin_->setRange(0, 2000);
    paddingSpin_->setSingleStep(100);
    paddingSpin_->setValue(500);
    paddingSpin_->setSuffix(" ms");
    audioLayout->addRow("块间重叠:", paddingSpin_);

    contentLayout->addWidget(audioGroup);

    // UI 设置
    auto* uiGroup = new QGroupBox("界面设置", contentWidget);
    auto* uiLayout = new QFormLayout(uiGroup);
    uiLayout->setSpacing(8);
    uiLayout->setContentsMargins(10, 12, 10, 12);

    themeCombo_ = new QComboBox(this);
    themeCombo_->addItems({"light", "dark"});
    uiLayout->addRow("主题:", themeCombo_);

    fontSizeSpin_ = new QSpinBox(this);
    fontSizeSpin_->setRange(10, 24);
    fontSizeSpin_->setValue(14);
    uiLayout->addRow("字体大小:", fontSizeSpin_);

    showWaveformCheck_ = new QCheckBox("显示波形", this);
    showWaveformCheck_->setChecked(true);
    uiLayout->addRow("波形显示:", showWaveformCheck_);

    showConfidenceCheck_ = new QCheckBox("显示置信度", this);
    showConfidenceCheck_->setChecked(true);
    uiLayout->addRow("置信度显示:", showConfidenceCheck_);

    contentLayout->addWidget(uiGroup);
    contentLayout->addStretch();

    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);

    // ---- 底部操作按钮（固定不滚动） ----
    auto* btnBar = new QWidget(this);
    auto* btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(12, 4, 12, 8);

    auto* saveBtn = new QPushButton("保存配置", btnBar);
    saveBtn->setStyleSheet("QPushButton { font-weight: bold; padding: 8px 16px; }");
    connect(saveBtn, &QPushButton::clicked, this, &SettingsPage::onSaveConfig);
    btnLayout->addWidget(saveBtn);

    auto* resetBtn = new QPushButton("恢复默认", btnBar);
    connect(resetBtn, &QPushButton::clicked, this, &SettingsPage::onResetConfig);
    btnLayout->addWidget(resetBtn);
    btnLayout->addStretch();

    statusLabel_ = new QLabel("配置未修改", btnBar);
    statusLabel_->setStyleSheet("color: gray;");
    btnLayout->addWidget(statusLabel_);

    mainLayout->addWidget(btnBar);
}

void SettingsPage::loadFromConfig() {
    logDirEdit_->setText(configManager_->get("app.log_dir").toString());
    modelPathEdit_->setText(configManager_->get("stt.model_path").toString());
    tokensPathEdit_->setText(configManager_->get("stt.tokens_path").toString());
    modelTypeCombo_->setCurrentText(configManager_->get("stt.model_type").toString());
    deviceCombo_->setCurrentText(configManager_->get("stt.device").toString());
    threadSpin_->setValue(configManager_->get("stt.num_threads").toInt());
    sampleRateSpin_->setValue(configManager_->get("stt.sample_rate").toInt());
    languageCombo_->setCurrentText(configManager_->get("stt.language").toString());
    streamingCheck_->setChecked(configManager_->get("stt.streaming").toBool());
    debugSaveAudioCheck_->setChecked(configManager_->get("stt.debug_save_audio").toBool());
    hotkeyRecorder_->setHotkeyText(configManager_->get("shortcuts.voice_hotkey").toString());
    beamSizeSpin_->setValue(configManager_->get("stt.beam_size").toInt());
    temperatureSpin_->setValue(configManager_->get("stt.temperature").toDouble());

    bufferSizeSpin_->setValue(configManager_->get("audio.buffer_size_ms").toInt());
    chunkDurationSpin_->setValue(configManager_->get("audio.chunk_duration_ms").toInt());
    paddingSpin_->setValue(configManager_->get("audio.padding_ms").toInt());

    // 恢复音频设备选择
    int savedDevice = configManager_->get("audio.input_device").toInt();
    selectAudioDevice(savedDevice);
    audioDebugDirEdit_->setText(configManager_->get("audio.debug_dir").toString());

    themeCombo_->setCurrentText(configManager_->get("ui.theme").toString());
    fontSizeSpin_->setValue(configManager_->get("ui.font_size").toInt());
    showWaveformCheck_->setChecked(configManager_->get("ui.show_waveform").toBool());
    showConfidenceCheck_->setChecked(configManager_->get("ui.show_confidence").toBool());
}

void SettingsPage::saveToConfig() {
    // 批量写入所有配置，只发射一次 configChanged 信号
    QMap<QString, QVariant> batch;
    batch["app.log_dir"] = logDirEdit_->text();
    batch["stt.model_path"] = modelPathEdit_->text();
    batch["stt.tokens_path"] = tokensPathEdit_->text();
    batch["stt.model_type"] = modelTypeCombo_->currentText();
    batch["stt.device"] = deviceCombo_->currentText();
    batch["stt.num_threads"] = threadSpin_->value();
    batch["stt.sample_rate"] = sampleRateSpin_->value();
    batch["stt.language"] = languageCombo_->currentText();
    batch["stt.streaming"] = streamingCheck_->isChecked();
    batch["stt.debug_save_audio"] = debugSaveAudioCheck_->isChecked();
    batch["stt.beam_size"] = beamSizeSpin_->value();
    batch["stt.temperature"] = temperatureSpin_->value();
    batch["shortcuts.voice_hotkey"] = hotkeyRecorder_->hotkeyText();
    batch["audio.input_device"] = getSelectedAudioDeviceIndex();
    batch["audio.debug_dir"] = audioDebugDirEdit_->text();
    batch["audio.buffer_size_ms"] = bufferSizeSpin_->value();
    batch["audio.chunk_duration_ms"] = chunkDurationSpin_->value();
    batch["audio.padding_ms"] = paddingSpin_->value();
    batch["ui.theme"] = themeCombo_->currentText();
    batch["ui.font_size"] = fontSizeSpin_->value();
    batch["ui.show_waveform"] = showWaveformCheck_->isChecked();
    batch["ui.show_confidence"] = showConfidenceCheck_->isChecked();

    configManager_->setBatch(batch);
}

void SettingsPage::onBrowseModelPath() {
    QString path = QFileDialog::getOpenFileName(this, "选择 ONNX 模型", "",
        "ONNX 模型 (*.onnx);;所有文件 (*.*)");
    if (!path.isEmpty()) {
        modelPathEdit_->setText(path);
    }
}

void SettingsPage::populateAudioDevices() {
    audioDeviceCombo_->clear();
    audioDeviceCombo_->addItem("默认设备", -1);

#ifdef HAVE_PORTAUDIO
    // 直接使用 PortAudio 枚举所有输入设备
    QStringList devices = AudioCapture::getDeviceList();
    // 跳过第一个 "默认设备"（已手动添加）
    for (int i = 1; i < devices.size(); i++) {
        audioDeviceCombo_->addItem(devices[i], i - 1); // display text, PortAudio index
    }
#else
    audioDeviceCombo_->addItem("PortAudio 未启用", -1);
#endif
}

void SettingsPage::selectAudioDevice(int deviceIndex) {
    // deviceIndex == -1 表示默认设备，对应 combo 的第一项（index 0）
    if (deviceIndex < 0) {
        audioDeviceCombo_->setCurrentIndex(0);
    } else {
        // 在 combo 中查找 data == deviceIndex 的项
        for (int i = 0; i < audioDeviceCombo_->count(); i++) {
            if (audioDeviceCombo_->itemData(i).toInt() == deviceIndex) {
                audioDeviceCombo_->setCurrentIndex(i);
                return;
            }
        }
        // 如果没找到，使用默认设备
        audioDeviceCombo_->setCurrentIndex(0);
    }
}

int SettingsPage::getSelectedAudioDeviceIndex() const {
    return audioDeviceCombo_->currentData().toInt();
}

void SettingsPage::onBrowseTokensPath() {
    QString path = QFileDialog::getOpenFileName(this, "选择词表文件", "",
        "词表文件 (tokens.txt);;所有文件 (*.*)");
    if (!path.isEmpty()) {
        tokensPathEdit_->setText(path);
    }
}

void SettingsPage::onBrowseAudioDebugDir() {
    QString path = QFileDialog::getExistingDirectory(this, "选择调试音频目录", "",
        QFileDialog::ShowDirsOnly);
    if (!path.isEmpty()) {
        audioDebugDirEdit_->setText(path);
    }
}

void SettingsPage::onBrowseLogDir() {
    QString path = QFileDialog::getExistingDirectory(this, "选择日志目录", "",
        QFileDialog::ShowDirsOnly);
    if (!path.isEmpty()) {
        logDirEdit_->setText(path);
    }
}

void SettingsPage::onSaveConfig() {
    saveToConfig();
    if (configManager_->save()) {
        statusLabel_->setText(QString("配置已保存到: %1").arg(configManager_->configPath()));
        LOG_INFO(kTag, QString("配置已持久化: %1").arg(configManager_->configPath()));
    } else {
        statusLabel_->setText("配置保存失败，请检查路径权限");
        LOG_ERROR(kTag, "配置持久化失败");
    }
}

void SettingsPage::onResetConfig() {
    auto reply = QMessageBox::question(this, "确认", "确定要恢复默认配置吗？",
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        configManager_->resetToDefaults();
        loadFromConfig();
        statusLabel_->setText("已恢复默认配置");
    }
}

} // namespace impress
