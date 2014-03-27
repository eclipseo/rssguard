// This file is part of RSS Guard.
//
// Copyright (C) 2011-2014 by Martin Rotter <rotter.martinos@gmail.com>
//
// RSS Guard is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// RSS Guard is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with RSS Guard. If not, see <http://www.gnu.org/licenses/>.

#include "network-web/webbrowser.h"

#include "definitions/definitions.h"
#include "network-web/webbrowsernetworkaccessmanager.h"
#include "network-web/webpage.h"
#include "network-web/webview.h"
#include "miscellaneous/skinfactory.h"
#include "miscellaneous/iconthemefactory.h"
#include "gui/formmain.h"
#include "gui/tabwidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QWebFrame>
#include <QWidgetAction>
#include <QSlider>
#include <QLabel>
#include <QProgressBar>
#include <QToolButton>


QList<WebBrowser*> WebBrowser::m_runningWebBrowsers;

WebBrowser::WebBrowser(QWidget *parent)
  : TabContent(parent),
    m_layout(new QVBoxLayout(this)),
    m_toolBar(new QToolBar(tr("Navigation panel"), this)),
    m_webView(new WebView(this)),
    m_txtLocation(new LocationLineEdit(this)),
    m_actionBack(m_webView->pageAction(QWebPage::Back)),
    m_actionForward(m_webView->pageAction(QWebPage::Forward)),
    m_actionReload(m_webView->pageAction(QWebPage::Reload)),
    m_actionStop(m_webView->pageAction(QWebPage::Stop)),
    m_activeNewspaperMode(false) {

  m_index = -1;

  // Add this new instance to the global list of web browsers.
  // NOTE: This is used primarily for dynamic icon theme switching.
  m_runningWebBrowsers.append(this);

  // Initialize the components and layout.
  initializeLayout();

  setTabOrder(m_txtLocation, m_toolBar);
  setTabOrder(m_toolBar, m_webView);

  createConnections();
  initializeZoomWidget();
}

void WebBrowser::initializeZoomWidget() {
  // Initializations.
  m_zoomButtons = new QWidget(this);
  QLabel *zoom_label = new QLabel(tr("Zoom  "), m_zoomButtons);
  QHBoxLayout *layout = new QHBoxLayout(m_zoomButtons);
  QToolButton *button_decrease = new QToolButton(m_zoomButtons);
  m_btnResetZoom = new QToolButton(m_zoomButtons);
  QToolButton *button_increase = new QToolButton(m_zoomButtons);

  // Set texts.
  button_decrease->setText("-");
  button_decrease->setToolTip(tr("Decrease zoom."));
  m_btnResetZoom->setText("100%");
  m_btnResetZoom->setToolTip(tr("Reset zoom to default."));
  button_increase->setText("+");
  button_increase->setToolTip(tr("Increase zoom."));

  // Setup layout.
  layout->addWidget(zoom_label);
  layout->addWidget(button_decrease);
  layout->addWidget(m_btnResetZoom);
  layout->addWidget(button_increase);
  layout->setSpacing(2);
  layout->setMargin(3);
  m_zoomButtons->setLayout(layout);

  // Make connections..
  connect(button_increase, SIGNAL(clicked()), this, SLOT(increaseZoom()));
  connect(button_decrease, SIGNAL(clicked()), this, SLOT(decreaseZoom()));
  connect(m_btnResetZoom, SIGNAL(clicked()), this, SLOT(resetZoom()));

  m_actionZoom = new QWidgetAction(this);
  m_actionZoom->setDefaultWidget(m_zoomButtons);
}

void WebBrowser::initializeLayout() {
  m_toolBar->setFloatable(false);
  m_toolBar->setMovable(false);
  m_toolBar->setAllowedAreas(Qt::TopToolBarArea);

  // Modify action texts.
  m_actionBack->setText(tr("Back"));
  m_actionBack->setToolTip(tr("Go back."));
  m_actionForward->setText(tr("Forward"));
  m_actionForward->setToolTip(tr("Go forward."));
  m_actionReload->setText(tr("Reload"));
  m_actionReload->setToolTip(tr("Reload current web page."));
  m_actionStop->setText(tr("Stop"));
  m_actionStop->setToolTip(tr("Stop web page loading."));

  // Add needed actions into toolbar.
  m_toolBar->addAction(m_actionBack);
  m_toolBar->addAction(m_actionForward);
  m_toolBar->addAction(m_actionReload);
  m_toolBar->addAction(m_actionStop);
  m_toolBar->addWidget(m_txtLocation);

  // Initialize dynamic progress bar which will be displayed
  // at the bottom of web browser.
  m_lblProgress = new QLabel(this);
  m_loadingProgress = new QProgressBar(this);
  m_loadingProgress->setFixedHeight(15);
  m_loadingProgress->setMinimum(0);
  m_loadingProgress->setMaximum(100);
  m_loadingProgress->setAttribute(Qt::WA_TranslucentBackground);

  m_loadingLayout = new QHBoxLayout();
  m_loadingLayout->setMargin(0);
  m_loadingLayout->addWidget(m_lblProgress, 0, Qt::AlignLeft|Qt::AlignVCenter);
  m_loadingProgress->setLayout(m_loadingLayout);

  // Setup layout.
  m_layout->addWidget(m_toolBar);
  m_layout->addWidget(m_webView);
  m_layout->addWidget(m_loadingProgress);
  m_layout->setMargin(0);
  m_layout->setSpacing(0);

  m_loadingProgress->hide();
}

void WebBrowser::onLoadingStarted() {
  m_loadingProgress->setValue(0);
  m_loadingProgress->show();
}

void WebBrowser::onLoadingProgress(int progress) {
  m_loadingProgress->setValue(progress);

  m_lblProgress->setText(tr(" %1 kB / %2 kB").
                         arg(m_webView->page()->bytesReceived() / 1000).
                         arg(m_webView->page()->totalBytes() / 1000));
}

void WebBrowser::onLoadingFinished(bool success) {
  m_loadingProgress->hide();
}

void WebBrowser::createConnections() {
  // When user confirms new url, then redirect to it.
  connect(m_txtLocation,SIGNAL(submitted(QString)), this, SLOT(navigateToUrl(QString)));
  // If new page loads, then update current url.
  connect(m_webView, SIGNAL(urlChanged(QUrl)), this, SLOT(updateUrl(QUrl)));

  // Connect this WebBrowser to global TabWidget.
  TabWidget *tab_widget = FormMain::instance()->tabWidget();
  connect(m_webView, SIGNAL(newTabRequested()), tab_widget, SLOT(addEmptyBrowser()));
  connect(m_webView, SIGNAL(linkMiddleClicked(QUrl)), tab_widget, SLOT(addLinkedBrowser(QUrl)));

  // Change location textbox status according to webpage status.
  connect(m_webView, SIGNAL(loadStarted()), this, SLOT(onLoadingStarted()));
  connect(m_webView, SIGNAL(loadProgress(int)), this, SLOT(onLoadingProgress(int)));
  connect(m_webView, SIGNAL(loadFinished(bool)), this, SLOT(onLoadingFinished(bool)));

  // Forward title/icon changes.
  connect(m_webView, SIGNAL(titleChanged(QString)), this, SLOT(onTitleChanged(QString)));
  connect(m_webView, SIGNAL(iconChanged()), this, SLOT(onIconChanged()));

  // Misc connections.
  connect(m_webView, SIGNAL(zoomFactorChanged()), this, SLOT(updateZoomGui()));
}

void WebBrowser::onIconChanged() {
  emit iconChanged(m_index, m_webView->icon());
}

void WebBrowser::onTitleChanged(const QString &new_title) {
  if (new_title.isEmpty()) {
    //: Webbrowser tab title when no title is available.
    emit titleChanged(m_index, tr("No title"));
  }
  else {
    emit titleChanged(m_index, new_title);
  }
}

void WebBrowser::updateUrl(const QUrl &url) { 
  m_txtLocation->setText(url.toString());
}

void WebBrowser::navigateToUrl(const QUrl &url) {
  if (url.isValid()) {
    m_webView->load(url);
  }
}

void WebBrowser::navigateToMessages(const QList<Message> &messages) {
  SkinFactory *factory = SkinFactory::instance();
  QString messages_layout;
  QString single_message_layout = factory->currentMarkup();

  foreach (const Message &message, messages) {
    messages_layout.append(single_message_layout.arg(message.m_title,
                                                     tr("Written by ") + (message.m_author.isEmpty() ?
                                                                            tr("uknown author") :
                                                                            message.m_author),
                                                     message.m_url,
                                                     message.m_contents,
                                                     message.m_created.toString(Qt::DefaultLocaleShortDate)));
  }

  QString layout_wrapper = factory->currentMarkupLayout().arg(messages.size() == 1 ?
                                                                messages.at(0).m_title :
                                                                tr("Newspaper view"),
                                                              messages_layout);

  m_webView->setHtml(layout_wrapper, QUrl(INTERNAL_URL_NEWSPAPER));
  emit iconChanged(m_index,
                   IconThemeFactory::instance()->fromTheme("item-newspaper"));
}

void WebBrowser::updateZoomGui() {
  m_btnResetZoom->setText(QString("%1%").arg(QString::number(m_webView->zoomFactor() * 100,
                                                             'f',
                                                             0)));
}

void WebBrowser::increaseZoom() {
  m_webView->increaseWebPageZoom();
  updateZoomGui();
}

void WebBrowser::decreaseZoom() {
  m_webView->decreaseWebPageZoom();
  updateZoomGui();
}

void WebBrowser::resetZoom() {
  m_webView->resetWebPageZoom();
  updateZoomGui();
}

void WebBrowser::navigateToUrl(const QString &textual_url) {
  // Prepare input url.
  QString better_url = textual_url;
  better_url = better_url.replace('\\', '/');

  navigateToUrl(QUrl::fromUserInput(better_url));
}

WebBrowser::~WebBrowser() {
  qDebug("Destroying WebBrowser instance.");

  // Remove this instance from the global list of web browsers.
  m_runningWebBrowsers.removeAll(this);

  // Delete members.
  delete m_layout;
  delete m_zoomButtons;
  delete m_actionZoom;
}

void WebBrowser::setupIcons() {
  m_actionBack->setIcon(IconThemeFactory::instance()->fromTheme("go-previous"));
  m_actionForward->setIcon(IconThemeFactory::instance()->fromTheme("go-next"));
  m_actionReload->setIcon(IconThemeFactory::instance()->fromTheme("go-refresh"));
  m_actionStop->setIcon(IconThemeFactory::instance()->fromTheme("go-stop"));
  m_webView->setupIcons();
}
