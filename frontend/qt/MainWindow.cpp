#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "AppEnv.h"
#include "DlgListSelect.h"
#include "DlgConvertOption.h"
#include "SimplePlaylistView.h"

#include "MidClickTabBar.hpp"
#include "CustomHeadTabWidget.hpp"
using namespace sqt;

#include <scx/Signal.hpp>
using namespace scx;

#include <util/MediaItem.h>
using namespace mous;

using namespace std;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_TimerUpdateUi(new QTimer),
    m_UpdateInterval(500),
    m_UsedPlaylistView(NULL),
    m_UsedMediaItem(NULL),
    m_SliderPlayingPreempted(false)
{
    ui->setupUi(this);    
    InitMousCore();
    InitMyUi();
    InitQtSlots();

    m_FrmTagEditor.RestoreUiStatus();
    const AppEnv* env = GlobalAppEnv::Instance();
    restoreGeometry(env->windowGeometry);
    restoreState(env->windowState);
}

MainWindow::~MainWindow()
{
    m_FrmTagEditor.WaitForLoadFinished();

    m_Player->SigFinished()->DisconnectReceiver(this);

    if (m_Player->Status() == PlayerStatus::Playing) {
        m_Player->Close();
    }
    if (m_TimerUpdateUi != NULL) {
        if (m_TimerUpdateUi->isActive())
            m_TimerUpdateUi->stop();
        delete m_TimerUpdateUi;
    }

    delete ui;

    ClearMousCore();
}

void MainWindow::closeEvent(QCloseEvent*)
{
    AppEnv* env = GlobalAppEnv::Instance();
    env->tagEditorSplitterState = m_FrmTagEditor.saveGeometry();
    env->windowGeometry = saveGeometry();
    env->windowState = saveState();

    m_FrmTagEditor.SaveUiStatus();
}

void MainWindow::InitMousCore()
{
    m_PluginManager = IPluginManager::Create();
    m_MediaLoader = IMediaLoader::Create();
    m_Player = IPlayer::Create();
    m_ConvFactory = IConvTaskFactory::Create();
    m_ParserFactory = ITagParserFactory::Create();

    m_PluginManager->LoadPluginDir(GlobalAppEnv::Instance()->pluginDir.toLocal8Bit().data());
    vector<string> pathList;
    m_PluginManager->DumpPluginPath(pathList);

    vector<const IPluginAgent*> packAgentList;
    vector<const IPluginAgent*> tagAgentList;
    m_PluginManager->DumpPluginAgent(packAgentList, PluginType::MediaPack);
    m_PluginManager->DumpPluginAgent(tagAgentList, PluginType::TagParser);

    m_MediaLoader->RegisterMediaPackPlugin(packAgentList);
    m_MediaLoader->RegisterTagParserPlugin(tagAgentList);

    vector<const IPluginAgent*> decoderAgentList;
    vector<const IPluginAgent*> encoderAgentList;
    vector<const IPluginAgent*> rendererAgentList;
    m_PluginManager->DumpPluginAgent(decoderAgentList, PluginType::Decoder);
    m_PluginManager->DumpPluginAgent(encoderAgentList, PluginType::Encoder);
    m_PluginManager->DumpPluginAgent(rendererAgentList, PluginType::Renderer);

    m_Player->RegisterRendererPlugin(rendererAgentList[0]);
    m_Player->RegisterDecoderPlugin(decoderAgentList);
    m_Player->SigFinished()->Connect(&MainWindow::SlotPlayerFinished, this);

    m_ConvFactory->RegisterDecoderPlugin(decoderAgentList);
    m_ConvFactory->RegisterEncoderPlugin(encoderAgentList);

    m_ParserFactory->RegisterTagParserPlugin(tagAgentList);
    m_FrmTagEditor.SetTagParserFactory(m_ParserFactory);

    qDebug() << ">> MediaPack count:" << packAgentList.size();
    qDebug() << ">> TagParser count:" << tagAgentList.size();
    qDebug() << ">> Decoder count:" << decoderAgentList.size();
    qDebug() << ">> Encoder count:" << encoderAgentList.size();
    qDebug() << ">> Renderer count:" << rendererAgentList.size();
}

void MainWindow::ClearMousCore()
{
    m_Player->SigFinished()->DisconnectReceiver(this);
    m_FrmTagEditor.SetTagParserFactory(NULL);

    m_Player->UnregisterAll();
    m_MediaLoader->UnregisterAll();
    m_ConvFactory->UnregisterAll();
    m_ParserFactory->UnregisterAll();
    m_PluginManager->UnloadAll();

    IPluginManager::Free(m_PluginManager);
    IMediaLoader::Free(m_MediaLoader);
    IPlayer::Free(m_Player);
    IConvTaskFactory::Free(m_ConvFactory);
    ITagParserFactory::Free(m_ParserFactory);
}

void MainWindow::InitMyUi()
{
    // Playing & Paused icon
    m_IconPlaying.addFile(QString::fromUtf8(":/img/resource/play.png"), QSize(), QIcon::Normal, QIcon::On);
    m_IconPaused.addFile(QString::fromUtf8(":/img/resource/pause.png"), QSize(), QIcon::Normal, QIcon::On);

    // Volume
    m_FrmToolBar.SliderVolume()->setValue(m_Player->Volume());

    // PlayList View
    m_TabBarPlaylist = new MidClickTabBar(this);
    m_TabWidgetPlaylist = new CustomHeadTabWidget(this);
    m_TabWidgetPlaylist->SetTabBar(m_TabBarPlaylist);
    m_TabWidgetPlaylist->setMovable(true);
    ui->layoutPlaylist->addWidget(m_TabWidgetPlaylist);

    // Status bar buttons
    m_BtnPreference = new QToolButton(ui->statusBar);
    m_BtnPreference->setAutoRaise(true);
    m_BtnPreference->setText("P");
    m_BtnPreference->setToolTip(tr("Preference"));

    ui->statusBar->addPermanentWidget(m_BtnPreference, 0);

    // Show default playlist
    SlotWidgetPlayListDoubleClick();

    m_Dock = new QDockWidget(tr("Metadata"));
    m_Dock->setObjectName("Dock");
    m_Dock->setWidget(&m_FrmTagEditor);
    addDockWidget(Qt::LeftDockWidgetArea, m_Dock);
    m_Dock->setFeatures(QDockWidget::NoDockWidgetFeatures | QDockWidget::DockWidgetMovable);

    ui->toolBar->addWidget(&m_FrmToolBar);
    ui->toolBar->setMovable(false);
    setContextMenuPolicy(Qt::NoContextMenu);
}

void MainWindow::InitQtSlots()
{
    connect(m_TimerUpdateUi, SIGNAL(timeout()), this, SLOT(SlotUpdateUi()));

    connect(m_FrmToolBar.BtnPlay(), SIGNAL(clicked()), this, SLOT(SlotBtnPlay()));
    connect(m_FrmToolBar.BtnPrev(), SIGNAL(clicked()), this, SLOT(SlotBtnPrev()));
    connect(m_FrmToolBar.BtnNext(), SIGNAL(clicked()), this, SLOT(SlotBtnNext()));

    connect(m_FrmToolBar.SliderVolume(), SIGNAL(valueChanged(int)), this, SLOT(SlotSliderVolumeValueChanged(int)));

    connect(m_FrmToolBar.SliderPlaying(), SIGNAL(sliderPressed()), this, SLOT(SlotSliderPlayingPressed()));
    connect(m_FrmToolBar.SliderPlaying(), SIGNAL(sliderReleased()), this, SLOT(SlotSliderPlayingReleased()));
    connect(m_FrmToolBar.SliderPlaying(), SIGNAL(valueChanged(int)), this, SLOT(SlotSliderPlayingValueChanged(int)));

    connect(m_TabBarPlaylist, SIGNAL(SigMidClick(int)), this, SLOT(SlotBarPlayListMidClick(int)));
    connect(m_TabWidgetPlaylist, SIGNAL(SigDoubleClick()), this, SLOT(SlotWidgetPlayListDoubleClick()));

    connect(&m_FrmTagEditor, SIGNAL(SigMediaItemChanged(const MediaItem&)), this, SLOT(SlotTagUpdated(const MediaItem&)));
}

void MainWindow::FormatTime(QString& str, int ms)
{
    int sec = ms/1000;
    str.sprintf("%.2d:%.2d", (int)(sec/60), (int)(sec%60));
}

/* MousCore slots */
void MainWindow::SlotPlayerFinished()
{
    QMetaObject::invokeMethod(this, "SlotUiPlayerFinished", Qt::QueuedConnection);
}

void MainWindow::SlotUiPlayerFinished()
{
    qDebug() << "Stopped!";
    if (m_UsedPlaylistView != NULL) {
        const MediaItem* item = m_UsedPlaylistView->NextItem();
        if (item != NULL)
            SlotPlayMediaItem(m_UsedPlaylistView, *item);
    }
}

/* Qt slots */
void MainWindow::SlotUpdateUi()
{
    //==== Update statusbar.
    int total = m_Player->RangeDuration();
    int ms = m_Player->OffsetMs();
    int hz = m_Player->SamleRate();
    int kbps = m_Player->BitRate();

    const QString& status = QString("%1 Hz | %2 Kbps | %3:%4/%5:%6").arg(hz).arg(kbps, 4).
            arg(ms/1000/60, 2, 10, QChar('0')).arg(ms/1000%60, 2, 10, QChar('0')).
            arg(total/1000/60, 2, 10, QChar('0')).arg(total/1000%60, 2, 10, QChar('0'));

    ui->statusBar->showMessage(status);

    //==== Update slider.
    if (!m_SliderPlayingPreempted) {
        int percent = (double)ms / total * m_FrmToolBar.SliderPlaying()->maximum();
        m_FrmToolBar.SliderPlaying()->setSliderPosition(percent);
    }
}

void MainWindow::SlotBtnPlay()
{
    qDebug() << m_Player->Status();

    switch (m_Player->Status()) {
    case PlayerStatus::Closed:
        if (m_UsedMediaItem != NULL) {
            m_Player->Open(m_UsedMediaItem->url);
            SlotBtnPlay();
        }
        break;

    case PlayerStatus::Playing:
        m_Player->Pause();
        m_TimerUpdateUi->stop();
        m_FrmToolBar.BtnPlay()->setIcon(m_IconPlaying);
        break;

    case PlayerStatus::Paused:
        m_TimerUpdateUi->start(m_UpdateInterval);
        m_Player->Resume();
        m_FrmToolBar.BtnPlay()->setIcon(m_IconPaused);
        break;

    case PlayerStatus::Stopped:
        m_TimerUpdateUi->start(m_UpdateInterval);
        if (m_UsedMediaItem->hasRange)
            m_Player->Play(m_UsedMediaItem->msBeg, m_UsedMediaItem->msEnd);
        else
            m_Player->Play();
        m_FrmToolBar.BtnPlay()->setIcon(m_IconPaused);
        break;
    }
}

void MainWindow::SlotBtnPrev()
{
    const mous::MediaItem* item = m_UsedPlaylistView->PrevItem();
    if (item != NULL)
        SlotPlayMediaItem(m_UsedPlaylistView, *item);
}

void MainWindow::SlotBtnNext()
{
    const mous::MediaItem* item = m_UsedPlaylistView->NextItem();
    if (item != NULL)
        SlotPlayMediaItem(m_UsedPlaylistView, *item);
}

void MainWindow::SlotSliderVolumeValueChanged(int val)
{
    m_Player->SetVolume(val);
}

void MainWindow::SlotSliderPlayingPressed()
{
    m_SliderPlayingPreempted = true;
}

void MainWindow::SlotSliderPlayingReleased()
{
    m_SliderPlayingPreempted = false;
}

void MainWindow::SlotSliderPlayingValueChanged(int val)
{
    if (!m_SliderPlayingPreempted)
        return;

    const double& percent = (double)val / m_FrmToolBar.SliderPlaying()->maximum();
    m_Player->SeekPercent(percent);
}

void MainWindow::SlotBarPlayListMidClick(int index)
{
    SimplePlaylistView* view = (SimplePlaylistView*)m_TabWidgetPlaylist->widget(index);
    m_TabWidgetPlaylist->removeTab(index);

    disconnect(view, 0, this, 0);

    delete view;

    m_TabBarPlaylist->setFocus();
}

void MainWindow::SlotWidgetPlayListDoubleClick()
{
    SimplePlaylistView* view = new SimplePlaylistView(this);
    view->SetMediaLoader(m_MediaLoader);
    view->SetClipboard(&m_Clipboard);

    connect(view, SIGNAL(SigPlayMediaItem(IPlaylistView*, const MediaItem&)),
            this, SLOT(SlotPlayMediaItem(IPlaylistView*, const MediaItem&)));
    connect(view, SIGNAL(SigConvertMediaItem(const MediaItem&)),
            this, SLOT(SlotConvertMediaItem(const MediaItem&)));
    connect(view, SIGNAL(SigConvertMediaItems(const QList<MediaItem>&)),
            this, SLOT(SlotConvertMediaItems(const QList<MediaItem>&)));

    m_TabWidgetPlaylist->addTab(view, QString::number(m_TabWidgetPlaylist->count()));
    m_TabWidgetPlaylist->setCurrentIndex(m_TabWidgetPlaylist->count()-1);
}

void MainWindow::SlotPlayMediaItem(IPlaylistView *view, const MediaItem& item)
{
    if (m_Player->Status() == PlayerStatus::Playing) {
        m_Player->Close();
    }
    if (m_Player->Status() != PlayerStatus::Closed) {
        m_Player->Close();
        m_TimerUpdateUi->stop();
    }

    m_UsedMediaItem = &item;

    m_Player->Open(item.url);
    m_TimerUpdateUi->start(m_UpdateInterval);
    if (item.hasRange)
        m_Player->Play(item.msBeg, item.msEnd);
    else
        m_Player->Play();
    m_FrmToolBar.BtnPlay()->setIcon(m_IconPaused);

    setWindowTitle(QString::fromUtf8(item.tag.title.c_str()));

    m_UsedPlaylistView = view;

    m_FrmTagEditor.LoadMediaItem(item);
}

void MainWindow::SlotConvertMediaItem(const MediaItem& item)
{
    //==== show encoders
    vector<string> encoderNames = m_ConvFactory->EncoderNames();
    if (encoderNames.empty())
        return;
    QStringList list;
    for (size_t i = 0; i < encoderNames.size(); ++i) {
        list << QString::fromUtf8(encoderNames[i].c_str());
        qDebug() << ">> encoder:" << i+1 << encoderNames[i].c_str();
    }

    DlgListSelect dlgEncoders(this);
    dlgEncoders.setWindowTitle(tr("Available Encoders"));
    dlgEncoders.SetItems(list);
    dlgEncoders.SetSelectedIndex(0);
    dlgEncoders.exec();

    if (dlgEncoders.result() != QDialog::Accepted)
        return;

    int encoderIndex = dlgEncoders.GetSelectedIndex();
    IConvTask* newTask = m_ConvFactory->CreateTask(item, encoderNames[encoderIndex]);
    if (newTask == NULL)
        return;

    //==== show options
    vector<const BaseOption*> opts;
    newTask->EncoderOptions(opts);

    QString fileName =
            QString::fromUtf8((item.tag.artist + " - " + item.tag.title + "." + newTask->EncoderFileSuffix()).c_str());
    DlgConvertOption dlgOption(this);
    dlgOption.SetDir(QDir::homePath());
    dlgOption.SetFileName(fileName);
    dlgOption.BindWidgetAndOption(opts);
    dlgOption.setWindowTitle(tr("Config"));
    dlgOption.exec();

    if (dlgOption.result() != QDialog::Accepted) {
        IConvTask::Free(newTask);
        return;
    }

    //==== do work
    m_DlgConvertTask.show();
    m_DlgConvertTask.AddTask(newTask, fileName);
}

void MainWindow::SlotConvertMediaItems(const QList<MediaItem>& items)
{

}

void MainWindow::SlotTagUpdated(const MediaItem& item)
{
    if (m_UsedPlaylistView != NULL)
        m_UsedPlaylistView->OnMediaItemUpdated(item);
}
