// For license of this file, see <object-root-folder>/LICENSE.md.

#include "formdownloadattachment.h"

#include "gui/guiutilities.h"
#include "miscellaneous/application.h"
#include "miscellaneous/iconfactory.h"
#include "miscellaneous/iofactory.h"
#include "network-web/downloader.h"

#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>

FormDownloadAttachment::FormDownloadAttachment(const QString& target_file, Downloader* downloader, QWidget* parent) : QDialog(parent) {
  m_ui.setupUi(this);

  GuiUtilities::applyDialogProperties(*this, qApp->icons()->fromTheme(QSL("mail-attachment")), tr("Downloading attachment..."));

  connect(m_ui.m_btnBox->button(QDialogButtonBox::StandardButton::Abort), &QPushButton::clicked, downloader, &Downloader::cancel);
  connect(downloader, &Downloader::completed, [downloader, target_file](QNetworkReply::NetworkError status, QByteArray contents) {
    if (status == QNetworkReply::NetworkError::NoError) {
      QString data = QJsonDocument::fromJson(contents).object()["data"].toString();

      if (!data.isEmpty()) {
        IOFactory::writeFile(target_file, QByteArray::fromBase64(data.toLocal8Bit(),
                                                                 QByteArray::Base64Option::Base64UrlEncoding));
      }
    }

    downloader->deleteLater();
  });
  connect(downloader, &Downloader::completed, this, &FormDownloadAttachment::close);
  connect(downloader, &Downloader::progress, [this](qint64 bytes_received, qint64 bytes_total) {
    if (m_ui.m_progressBar->maximum() == 0) {
      return;
    }

    if (bytes_total <= 0) {
      m_ui.m_progressBar->setMinimum(0);
      m_ui.m_progressBar->setMaximum(0);
      m_ui.m_progressBar->setValue(0);
    }
    else {
      m_ui.m_progressBar->setValue(int((bytes_received * 100.0) / bytes_total));
    }
  });
}
