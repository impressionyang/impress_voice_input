#include "settings_page.h"
#include "app/config_manager.h"
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

    // STT 设置
    auto* sttGroup = new QGroupBox("STT 推理设置", this);
    auto* sttLayout = new QFormLayout(sttGroup);

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

    beamSizeSpin_ = new QSpinBox(this);
    beamSizeSpin_->setRange(1, 20);
    beamSizeSpin_->setValue(5);
    sttLayout->addRow("Beam Size:", beamSizeSpin_);

    temperatureSpin_ = new QDoubleSpinBox(this);
    temperatureSpin_->setRange(0.0, 2.0);
    temperatureSpin_->setSingleStep(0.1);
    temperatureSpin_->setValue(0.0);
    sttLayout->addRow("温度 (Temperature):", temperatureSpin_);

    mainLayout->addWidget(sttGroup);

    // 音频设置
    auto* audioGroup = new QGroupBox("音频设置", this);
    auto* audioLayout = new QFormLayout(audioGroup);

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

    mainLayout->addWidget(audioGroup);

    // UI 设置
    auto* uiGroup = new QGroupBox("界面设置", this);
    auto* uiLayout = new QFormLayout(uiGroup);

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

    mainLayout->addWidget(uiGroup);

    // 操作按钮
    auto* btnLayout = new QHBoxLayout();
    auto* saveBtn = new QPushButton("保存配置", this);
    saveBtn->setStyleSheet("QPushButton { font-weight: bold; padding: 8px 16px; }");
    connect(saveBtn, &QPushButton::clicked, this, &SettingsPage::onSaveConfig);
    btnLayout->addWidget(saveBtn);

    auto* resetBtn = new QPushButton("恢复默认", this);
    connect(resetBtn, &QPushButton::clicked, this, &SettingsPage::onResetConfig);
    btnLayout->addWidget(resetBtn);
    btnLayout->addStretch();

    statusLabel_ = new QLabel("配置未修改", this);
    statusLabel_->setStyleSheet("color: gray;");
    btnLayout->addWidget(statusLabel_);

    mainLayout->addLayout(btnLayout);
    mainLayout->addStretch();
}

void SettingsPage::loadFromConfig() {
    modelPathEdit_->setText(configManager_->get("stt.model_path").toString());
    tokensPathEdit_->setText(configManager_->get("stt.tokens_path").toString());
    modelTypeCombo_->setCurrentText(configManager_->get("stt.model_type").toString());
    deviceCombo_->setCurrentText(configManager_->get("stt.device").toString());
    threadSpin_->setValue(configManager_->get("stt.num_threads").toInt());
    sampleRateSpin_->setValue(configManager_->get("stt.sample_rate").toInt());
    languageCombo_->setCurrentText(configManager_->get("stt.language").toString());
    streamingCheck_->setChecked(configManager_->get("stt.streaming").toBool());
    beamSizeSpin_->setValue(configManager_->get("stt.beam_size").toInt());
    temperatureSpin_->setValue(configManager_->get("stt.temperature").toDouble());

    bufferSizeSpin_->setValue(configManager_->get("audio.buffer_size_ms").toInt());
    chunkDurationSpin_->setValue(configManager_->get("audio.chunk_duration_ms").toInt());
    paddingSpin_->setValue(configManager_->get("audio.padding_ms").toInt());

    themeCombo_->setCurrentText(configManager_->get("ui.theme").toString());
    fontSizeSpin_->setValue(configManager_->get("ui.font_size").toInt());
    showWaveformCheck_->setChecked(configManager_->get("ui.show_waveform").toBool());
    showConfidenceCheck_->setChecked(configManager_->get("ui.show_confidence").toBool());
}

void SettingsPage::saveToConfig() {
    configManager_->set("stt.model_path", modelPathEdit_->text());
    configManager_->set("stt.tokens_path", tokensPathEdit_->text());
    configManager_->set("stt.model_type", modelTypeCombo_->currentText());
    configManager_->set("stt.device", deviceCombo_->currentText());
    configManager_->set("stt.num_threads", threadSpin_->value());
    configManager_->set("stt.sample_rate", sampleRateSpin_->value());
    configManager_->set("stt.language", languageCombo_->currentText());
    configManager_->set("stt.streaming", streamingCheck_->isChecked());
    configManager_->set("stt.beam_size", beamSizeSpin_->value());
    configManager_->set("stt.temperature", temperatureSpin_->value());

    configManager_->set("audio.buffer_size_ms", bufferSizeSpin_->value());
    configManager_->set("audio.chunk_duration_ms", chunkDurationSpin_->value());
    configManager_->set("audio.padding_ms", paddingSpin_->value());

    configManager_->set("ui.theme", themeCombo_->currentText());
    configManager_->set("ui.font_size", fontSizeSpin_->value());
    configManager_->set("ui.show_waveform", showWaveformCheck_->isChecked());
    configManager_->set("ui.show_confidence", showConfidenceCheck_->isChecked());
}

void SettingsPage::onBrowseModelPath() {
    QString path = QFileDialog::getOpenFileName(this, "选择 ONNX 模型", "",
        "ONNX 模型 (*.onnx);;所有文件 (*.*)");
    if (!path.isEmpty()) {
        modelPathEdit_->setText(path);
    }
}

void SettingsPage::onBrowseTokensPath() {
    QString path = QFileDialog::getOpenFileName(this, "选择词表文件", "",
        "词表文件 (tokens.txt);;所有文件 (*.*)");
    if (!path.isEmpty()) {
        tokensPathEdit_->setText(path);
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
