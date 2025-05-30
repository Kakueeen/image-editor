// SPDX-FileCopyrightText: 2020 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "navigationwidget.h"
#include "widgets/imagebutton.h"
#include "unionimage/baseutils.h"

#include <QPainter>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <QtDebug>
#include <QRgb>

#include <DIconButton>
#include <DDialogCloseButton>
#include <DGuiApplicationHelper>
#include <dwindowclosebutton.h>

#include "service/configsetter.h"
namespace {

const QString SETTINGS_GROUP = "VIEWPANEL";
const QString SETTINGS_ALWAYSHIDDEN_KEY = "NavigationAlwaysHidden";
const int IMAGE_MARGIN = 5;
const int IMAGE_MARGIN_BOTTOM = 5;
//因为qrc改变,需要改变资源文件的获取路径,bug63261
const QString ICON_CLOSE_NORMAL_LIGHT = ":/light/images/button_tab_close_normal 2.svg";
const QString ICON_CLOSE_HOVER_LIGHT = ":/light/images/button_tab_close_hover 2.svg";
const QString ICON_CLOSE_PRESS_LIGHT = ":/light/images/button_tab_close_press 2.svg";
const QString ICON_CLOSE_NORMAL_DARK = ":/dark/images/button_tab_close_normal 3.svg";
const QString ICON_CLOSE_HOVER_DARK = ":/dark/images/button_tab_close_hover 3.svg";
const QString ICON_CLOSE_PRESS_DARK = ":/dark/images/button_tab_close_press 3.svg";

}  // namespace

using namespace Dtk::Widget;

NavigationWidget::NavigationWidget(QWidget *parent)
    : QWidget(parent)
{
    qDebug() << "Initializing NavigationWidget";
    hide();
    resize(150, 112);
    qDebug() << "NavigationWidget size set to:" << QSize(150, 112);

    ImageButton *closeBtn_light = new ImageButton(ICON_CLOSE_NORMAL_LIGHT, ICON_CLOSE_HOVER_LIGHT, ICON_CLOSE_PRESS_LIGHT, " ", this);
    closeBtn_light->setTooltipVisible(true);
    closeBtn_light->setFixedSize(32, 32);
    closeBtn_light->move(QPoint(this->x() + this->width() - 27 - 10,
                                rect().topRight().y() + 4 - 3));
    DPalette palette1 ;
    palette1.setColor(DPalette::Window, QColor(0, 0, 0, 1));
    closeBtn_light->setPalette(palette1);
    closeBtn_light->hide();
    connect(closeBtn_light, &ImageButton::clicked, [this]() {
        setAlwaysHidden(true);
    });

    ImageButton *closeBtn_dark = new ImageButton(ICON_CLOSE_NORMAL_DARK, ICON_CLOSE_HOVER_DARK, ICON_CLOSE_PRESS_DARK, " ", this);
    closeBtn_dark->setTooltipVisible(true);
    closeBtn_dark->setFixedSize(32, 32);
    closeBtn_dark->move(QPoint(this->x() + this->width() - 27 - 10,
                               rect().topRight().y() + 4 - 3));
    DPalette palette2 ;
    palette2.setColor(DPalette::Window, QColor(0, 0, 0, 1));
    closeBtn_dark->setPalette(palette2);
    closeBtn_dark->hide();
    connect(closeBtn_dark, &ImageButton::clicked, [this]() {
        setAlwaysHidden(true);
    });

    //修复style问题
    if (DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::DarkType) {
        qDebug() << "Setting up dark theme";
        closeBtn_light->hide();
        closeBtn_dark->show();
        m_bgImgUrl = Libutils::view::naviwindow::DARK_BG_IMG ;
        m_BgColor = Libutils::view::naviwindow::DARK_BG_COLOR;
        m_mrBgColor = Libutils::view::naviwindow::DARK_MR_BG_COLOR;
        m_mrBorderColor = Libutils::view::naviwindow::DARK_MR_BORDER_Color;
        m_imgRBorderColor = Libutils::view::naviwindow:: DARK_IMG_R_BORDER_COLOR;
    } else {
        qDebug() << "Setting up light theme";
        closeBtn_dark->hide();
        closeBtn_light->show();
        m_bgImgUrl = Libutils::view::naviwindow::LIGHT_BG_IMG ;
        m_BgColor = Libutils::view::naviwindow::LIGHT_BG_COLOR;
        m_mrBgColor = Libutils::view::naviwindow::LIGHT_MR_BG_COLOR;
        m_mrBorderColor = Libutils::view::naviwindow::LIGHT_MR_BORDER_Color;
        m_imgRBorderColor = Libutils::view::naviwindow::LIGHT_IMG_R_BORDER_COLOR;
    }

    QObject::connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, [ = ]() {
        DGuiApplicationHelper::ColorType themeType = DGuiApplicationHelper::instance()->themeType();
        if (themeType == DGuiApplicationHelper::DarkType) {
            qDebug() << "Theme changed to dark";
            closeBtn_light->hide();
            closeBtn_dark->show();
            m_bgImgUrl = Libutils::view::naviwindow::DARK_BG_IMG ;
            m_BgColor = Libutils::view::naviwindow::DARK_BG_COLOR;
            m_mrBgColor = Libutils::view::naviwindow::DARK_MR_BG_COLOR;
            m_mrBorderColor = Libutils::view::naviwindow::DARK_MR_BORDER_Color;
            m_imgRBorderColor = Libutils::view::naviwindow:: DARK_IMG_R_BORDER_COLOR;
        } else {
            qDebug() << "Theme changed to light";
            closeBtn_dark->hide();
            closeBtn_light->show();
            m_bgImgUrl = Libutils::view::naviwindow::LIGHT_BG_IMG ;
            m_BgColor = Libutils::view::naviwindow::LIGHT_BG_COLOR;
            m_mrBgColor = Libutils::view::naviwindow::LIGHT_MR_BG_COLOR;
            m_mrBorderColor = Libutils::view::naviwindow::LIGHT_MR_BORDER_Color;
            m_imgRBorderColor = Libutils::view::naviwindow::LIGHT_IMG_R_BORDER_COLOR;
        }
    });

    m_mainRect = QRect(rect().x() + IMAGE_MARGIN,
                       rect().y() + IMAGE_MARGIN_BOTTOM,
                       rect().width() - IMAGE_MARGIN * 2,
                       rect().height() - IMAGE_MARGIN_BOTTOM * 2);
    qDebug() << "Main rectangle set to:" << m_mainRect;

//    connect(dApp->viewerTheme, &ViewerThemeManager::viewerThemeChanged, this,
//            &NavigationWidget::onThemeChanged);
}

void NavigationWidget::setAlwaysHidden(bool value)
{
    qDebug() << "Setting always hidden to:" << value;
    LibConfigSetter::instance()->setValue(SETTINGS_GROUP, SETTINGS_ALWAYSHIDDEN_KEY,
                                          QVariant(value));
    if (isAlwaysHidden())
        hide();
    else
        show();
}

bool NavigationWidget::isAlwaysHidden() const
{
    return LibConfigSetter::instance()->value(SETTINGS_GROUP, SETTINGS_ALWAYSHIDDEN_KEY,
                                              QVariant(false)).toBool();
}

QPoint NavigationWidget::transImagePos(QPoint pos)
{
    return pos - QPoint(imageDrawRect.x(), imageDrawRect.y());
}

void NavigationWidget::setImage(const QImage &img)
{
    qDebug() << "Setting navigation image, original size:" << img.size();
    const qreal ratio = devicePixelRatioF();

    QRect tmpImageRect = QRect(m_mainRect.x(), m_mainRect.y(),
                               qRound(m_mainRect.width() * ratio),
                               qRound(m_mainRect.height() * ratio));

    m_originRect = img.rect();

    // 只在图片比可显示区域大时才缩放
    if (tmpImageRect.width() < m_originRect.width() || tmpImageRect.height() < m_originRect.height()) {
        qDebug() << "Scaling image to fit display area";
        m_img = img.scaled(tmpImageRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        m_img = img;
    }
    //修复尺寸如果接近当前框体尺寸,会出现出界的情况
    QImage tmpImg = m_img;

    //适应缩放比例
    if (m_img.height() > (tmpImageRect.height() - 20) && m_img.width() >= (tmpImageRect.width() - 10)) {
        qDebug() << "Adjusting image height to fit display area";
        m_img = m_img.scaled(m_img.width(), tmpImageRect.height() - 20);
    } else if (m_img.height() > (tmpImageRect.height() - 10) && m_img.width() > (tmpImageRect.width() - 25)) {
        qDebug() << "Adjusting image width to fit display area";
        m_img = m_img.scaled((tmpImageRect.width() - 25), m_img.height());
    }

    m_widthScale = qreal(m_img.width()) / qreal(tmpImg.width());
    m_heightScale = qreal(m_img.height()) / qreal(tmpImg.height());
    m_pix = QPixmap::fromImage(m_img);
    m_pix.setDevicePixelRatio(ratio);

    m_imageScale = qMax(1.0, qMax(qreal(img.width()) / qreal(m_img.width()), qreal(img.height()) / qreal(m_img.height())));
    m_r = QRectF(0, 0, m_img.width(), m_img.height());

    imageDrawRect = QRect((m_mainRect.width() - m_img.width() / ratio) / 2 + IMAGE_MARGIN,
                          (m_mainRect.height() - m_img.height() / ratio) / 2 + Libutils::common::BORDER_WIDTH,
                          m_img.width() / ratio, m_img.height() / ratio);
    qDebug() << "Image processing completed, final size:" << m_img.size() << "scale:" << m_imageScale;

    update();
}

void NavigationWidget::setRectInImage(const QRect &r)
{
    if (m_img.isNull()) {
        qWarning() << "Cannot set rect in image: image is null";
        return;
    }
    qDebug() << "Setting rect in image:" << r;
    m_r.setX(qreal(r.x()) / m_imageScale / m_heightScale);
    m_r.setY(qreal(r.y()) / m_imageScale / m_widthScale);
    m_r.setWidth(qreal(r.width()) / m_imageScale / m_heightScale);
    m_r.setHeight(qreal(r.height()) / m_imageScale / m_widthScale);

    update();
}

void NavigationWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        qDebug() << "Mouse press at position:" << e->pos();
        tryMoveRect(transImagePos(e->pos()));
    }
}

void NavigationWidget::mouseMoveEvent(QMouseEvent *e)
{
    qDebug() << "Mouse move to position:" << e->pos();
    tryMoveRect(transImagePos(e->pos()));
}

void NavigationWidget::tryMoveRect(const QPoint &p)
{
    if (!m_mainRect.contains(p)) {
        qDebug() << "Point" << p << "is outside main rectangle";
        return;
    }

    //修复鼠标位置存在出入的问题
    qreal x = p.x() * m_imageScale * m_heightScale;
    qreal y = p.y() * m_imageScale * m_widthScale;
    qDebug() << "Moving rect to position:" << QPointF(x, y);

    Q_EMIT requestMove(x, y);
}

bool NavigationWidget::checkbgisdark(QImage &img) const
{
    qDebug() << "Checking if background is dark";
    int npixcntx, npixcnty;
    bool numlessflag;
    m_r.toRect().width() * m_r.toRect().height() < 50 ? numlessflag = true : numlessflag = false;
    if (numlessflag) {
        npixcntx = m_r.toRect().width();
        npixcnty = m_r.toRect().height();
    } else {
        npixcntx = m_r.toRect().width() / 5;
        npixcnty = m_r.toRect().height() / 5;
    }

    int total = 0;
    int darktotal = 0;
    for (int i = 0; i < npixcntx; i++) {
        for (int j = 0; j < npixcnty; j++) {
            total++;
            QRgb rgb;
            if (numlessflag)
                rgb = img.pixel(m_r.toRect().x(), m_r.toRect().y());
            else {
                rgb = img.pixel(m_r.toRect().x() + 5 * i, m_r.toRect().y() + 5 * j);
            }
            int red = qRed(rgb);
            int green = qGreen(rgb);
            int blue = qGreen(rgb);
            int gray = (red * 30 + green * 59 + blue * 11) / 100;
            if (gray < 25) {
                darktotal++;
            }
        }
    }
    bool isDark = darktotal / (total * 1.00) > 0.95;
    qDebug() << "Background darkness check result:" << isDark << "dark pixels:" << darktotal << "total pixels:" << total;
    return isDark;
}

void NavigationWidget::paintEvent(QPaintEvent *)
{
    QImage img(m_img);
    if (m_img.isNull()) {
        qDebug() << "Painting empty navigation widget";
        QPainter p(this);
        p.fillRect(m_r, m_BgColor);
        return;
    }

    qDebug() << "Painting navigation widget with image size:" << img.size();
    QPainter p(&img);
    p.fillRect(m_r, m_mrBgColor);
    if (checkbgisdark(img)) {
        qDebug() << "Using gray pen for dark background";
        p.setPen(QPen(Qt::gray));
    } else {
        qDebug() << "Using white pen for light background";
        p.setPen(QColor(Qt::white));
    }

    // 遮罩框白色描边
    if (m_r.right() >= img.width())
        p.drawRect(m_r.adjusted(0, 0, -1, 0));
    else
        p.drawRect(m_r);
    p.end();
    p.begin(this);
    QImage background(m_bgImgUrl);
    p.drawImage(this->rect(), background);
    p.drawImage(imageDrawRect, img);
    QRect borderRect = QRect(imageDrawRect.x(), imageDrawRect.y() + 1, imageDrawRect.width(), imageDrawRect.height() + 1);
    p.setPen(QColor(0, 0, 0, 0));
    p.drawRect(borderRect);
    p.end();
}

//void NavigationWidget::onThemeChanged(ViewerThemeManager::AppTheme theme)
//{
//    if (theme == ViewerThemeManager::Dark) {
//        m_bgImgUrl = utils::view::naviwindow::DARK_BG_IMG ;
//        m_BgColor = utils::view::naviwindow::DARK_BG_COLOR;
//        m_mrBgColor = utils::view::naviwindow::DARK_MR_BG_COLOR;
//        m_mrBorderColor = utils::view::naviwindow::DARK_MR_BORDER_Color;
//        m_imgRBorderColor = utils::view::naviwindow:: DARK_IMG_R_BORDER_COLOR;
//    } else {
//        m_bgImgUrl = utils::view::naviwindow::LIGHT_BG_IMG ;
//        m_BgColor = utils::view::naviwindow::LIGHT_BG_COLOR;
//        m_mrBgColor = utils::view::naviwindow::LIGHT_MR_BG_COLOR;
//        m_mrBorderColor = utils::view::naviwindow::LIGHT_MR_BORDER_Color;
//        m_imgRBorderColor = utils::view::naviwindow::LIGHT_IMG_R_BORDER_COLOR;
//    }
//    update();
//}
