// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mtpfileproxy.h"
#include "imageengine.h"

#include <QRegularExpression>
#include <QStorageInfo>
#include <QDebug>

#include <algorithm>

#ifdef USE_DFM_IO
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#   include <dfm-io/doperator.h>
#else
#   include <dfm6-io/dfm-io/doperator.h>
#endif
USING_IO_NAMESPACE
#endif

static QString realPath(const QStringList &paths, const QString &filePath)
{
    return filePath.isEmpty() && !paths.isEmpty() ? paths.first() : filePath;
}

/**
   @brief 接收拷贝完成信号
 */
static void fileOperateCallbackFunc(bool ret, void *data)
{
    if (data) {
        qDebug() << "File operation callback received - Success:" << ret;
        MtpFileProxy::instance()->loadFinished(*reinterpret_cast<QString *>(data), ret);
    }
}

MtpFileProxy::MtpFileProxy()
{
#ifdef USE_DFM_IO
    qInfo() << "Initializing MtpFileProxy with dfm-io support";
#else
    qInfo() << "Initializing MtpFileProxy with QFile support";
#endif
}

MtpFileProxy::~MtpFileProxy() 
{
    qDebug() << "MtpFileProxy destructor called";
}

MtpFileProxy *MtpFileProxy::instance()
{
    static MtpFileProxy ins;
    return &ins;
}

/**
   @return 返回当前是否存在代理文件
 */
bool MtpFileProxy::isValid() const
{
    return !proxyCache.isEmpty();
}

/**
   @brief 检测传入的文件 `firstPath` 和列表 `paths` 是否为MTP文件。
    1. 若为，则创建代理文件，并将 `firstPath` `paths` 替换为代理文件路径，返回 true ;
    2. 若非MTP文件，则不修改传入参数，返回 false .
 */
bool MtpFileProxy::checkAndCreateProxyFile(QStringList &paths, QString &firstPath)
{
    firstPath = realPath(paths, firstPath);
    qDebug() << "Checking file for MTP device:" << firstPath;
    
    if (MtpFileProxy::instance()->checkFileDeviceIsMtp(firstPath)) {
        firstPath = MtpFileProxy::instance()->createPorxyFile(firstPath);
        paths.clear();
        paths.append(firstPath);

        qInfo() << "MTP mount file detected and proxy created:" << firstPath;
        return true;
    }

    return false;
}

/**
   @return 返回文件 `filePath` 是否为MTP挂载图片文件
    目前可能采用 gvfs/cifs 进行挂载。
 */
bool MtpFileProxy::checkFileDeviceIsMtp(const QString &filePath)
{
    QStorageInfo storage(filePath);
    qDebug() << "Checking storage device for:" << filePath << "Device:" << storage.device();
    
    if (storage.device().startsWith("gvfs") || storage.device().startsWith("cifs")) {
        // /run/user/1000/gvfs/mtp: /run/user/1000/gvfs/gphoto2:
        QString absoluteFilePath = QFileInfo(filePath).absoluteFilePath();
        if (absoluteFilePath.contains(QRegularExpression("fs/(mtp|gphoto2):"))) {
            bool isImage = ImageEngine::instance()->isImage(filePath);
            qDebug() << "File is on MTP device:" << isImage;
            return isImage;
        }
    }

    return false;
}

/**
   @brief 提交代理文件 `proxyFile` 变更(图片旋转)到 MTP 挂载文件并返回是否成功。
 */
bool MtpFileProxy::submitChangesToMTP(const QString &proxyFile)
{
    if (isValid() && proxyCache.contains(proxyFile)) {
        auto infoPtr = proxyCache.value(proxyFile);
        qDebug() << "Submitting changes to MTP file:" << proxyFile << "->" << infoPtr->originFileName;

        // 提交临时文件变更到 MTP 原始文件目录
#ifdef USE_DFM_IO
        DOperator copyOpt(QUrl::fromLocalFile(proxyFile));
        if (!copyOpt.copyFile(QUrl::fromLocalFile(infoPtr->originFileName), DFile::CopyFlag::kOverwrite)) {
            qWarning() << "Failed to submit changes to MTP file using DOperator. Error:" 
                      << copyOpt.lastError().errorMsg();
            return false;
        }
#else
        QFile copyFile(proxyFile);
        if (!copyFile.copy(infoPtr->originFileName)) {
            qWarning() << "Failed to submit changes to MTP file using QFile. Error:" 
                      << copyFile.errorString();
            return false;
        }
#endif
        qInfo() << "Successfully submitted changes to MTP file";
        return true;
    }

    qWarning() << "Cannot submit changes: Invalid proxy file or cache not found:" << proxyFile;
    return false;
}

/**
   @brief 返回当前MTP文件代理是否支持DFM IO(异步)，反之则使用 Qt 默认拷贝方式。
 */
bool MtpFileProxy::supportDFMIO() const
{
#ifdef USE_DFM_IO
    return true;
#else
    return false;
#endif
}

/**
   @return 返回缓存是否包含 `proxyFile` 代理文件路径
 */
bool MtpFileProxy::contains(const QString &proxyFile) const
{
    return proxyCache.contains(proxyFile);
}

/**
   @return 返回代理文件当前状态。
   @li None 初始状态
   @li Loading 加载中
   @li LoadSucc 加载成功
   @li LoadFailed 加载失败
   @li FileDelete 文件删除
 */
MtpFileProxy::FileState MtpFileProxy::state(const QString &proxyFile) const
{
    if (proxyCache.contains(proxyFile)) {
        return proxyCache.value(proxyFile)->fileState;
    }

    return None;
}

/**
   @brief 根据原始文件路径 `filePath` 创建代理文件并返回代理文件路径。
        如果已存在 `filePath` 的代理文件，将判断文件是否变更。
        创建并非立即完成，创建完成信号通过 `createProxyFileFinished` 发出。
 */
QString MtpFileProxy::createPorxyFile(const QString &filePath)
{
    qDebug() << "Creating proxy file for:" << filePath;
    
    auto findItr = std::find_if(proxyCache.begin(), proxyCache.end(), [&](const QSharedPointer<ProxyInfo> &info) {
        return info->originFileName == filePath;
    });

    if (findItr != proxyCache.end()) {
        // 同一路径文件已变更，移除之前缓存信息
        QFileInfo current(filePath);
        if (findItr.value()->lastModified == current.lastModified()) {
            qDebug() << "Using existing proxy file:" << findItr.key();
            return findItr.key();
        }
        qDebug() << "Removing outdated proxy file:" << findItr.key();
        proxyCache.erase(findItr);
    }

    QSharedPointer<ProxyInfo> infoPtr = QSharedPointer<ProxyInfo>(new ProxyInfo);
    if (!infoPtr->tempDir.isValid()) {
        qWarning() << "Failed to create temporary directory for MTP file";
        return filePath;
    }

    QFileInfo info(filePath);
    QString proxyFile = infoPtr->tempDir.filePath(info.fileName());
    infoPtr->fileState = Loading;
    infoPtr->proxyFileName = proxyFile;
    infoPtr->originFileName = filePath;
    infoPtr->lastModified = info.lastModified();
    proxyCache.insert(proxyFile, infoPtr);

    // 异步拷贝文件到临时目录
    qDebug() << "Created new proxy file:" << proxyFile;
    copyFileFromMtpAsync(infoPtr);

    return proxyFile;
}

/**
   @return 返回代理文件 `proxyFile` 所指向的原始文件路径。
 */
QString MtpFileProxy::mapToOriginFile(const QString &proxyFile) const
{
    if (proxyCache.contains(proxyFile)) {
        return proxyCache.value(proxyFile)->originFileName;
    }

    // 返回传入文件路径
    return proxyFile;
}

/**
   @brief 接收 dfm-io 异步拷贝文件 `proxyFile` 的结果 `ret`，将通过 `createProxyFileFinished` 信号通知
 */
void MtpFileProxy::loadFinished(const QString &proxyFile, bool ret)
{
    if (proxyCache.contains(proxyFile)) {
        if (!ret) {
            qWarning() << "Failed to copy MTP file to temporary folder:" << proxyFile;
        } else {
            qDebug() << "Successfully copied MTP file to temporary folder:" << proxyFile;
        }

        proxyCache.value(proxyFile)->fileState = ret ? LoadSucc : LoadFailed;
        Q_EMIT createProxyFileFinished(proxyFile, ret);
    } else {
        qWarning() << "Received load finished for unknown proxy file:" << proxyFile;
    }
}

/**
   @brief 触发原始文件 `originFile` 变更操作，将根据原始文件状态更新代理文件状态。
 */
void MtpFileProxy::triggerOriginFileChanged(const QString &originFile)
{
    qDebug() << "Checking origin file changes:" << originFile;
    
    auto findItr = std::find_if(proxyCache.begin(), proxyCache.end(), [&](const QSharedPointer<ProxyInfo> &info) {
        return info->originFileName == originFile;
    });

    if (findItr == proxyCache.end()) {
        qDebug() << "No proxy found for origin file:" << originFile;
        return;
    }

    QFileInfo info(originFile);
    auto proxyPtr = findItr.value();

    if (!info.exists()) {
        // 文件已被移除
        qInfo() << "Origin file has been deleted:" << originFile;
        if (QFile::rename(proxyPtr->proxyFileName, proxyPtr->proxyFileName + ".delete")) {
            proxyPtr->fileState = FileDelete;
        } else {
            qWarning() << "Failed to rename proxy file for deletion:" << proxyPtr->proxyFileName;
        }
    } else if (FileDelete == findItr.value()->fileState) {
        // 文件恢复
        qInfo() << "Origin file has been restored:" << originFile;
        if (QFile::rename(proxyPtr->proxyFileName + ".delete", proxyPtr->proxyFileName)) {
            proxyPtr->fileState = LoadSucc;
        } else {
            qWarning() << "Failed to rename proxy file for restoration:" << proxyPtr->proxyFileName;
        }
    } else if (info.lastModified() != findItr.value()->lastModified) {
        // 文件变更
        qInfo() << "Origin file has been modified:" << originFile;
        copyFileFromMtpAsync(proxyPtr);
        proxyPtr->lastModified = info.lastModified();
    }
}

/**
   @brief 根据给出的代理信息 `proxyPtr` ，将原始文件拷贝至临时文件目录
 */
void MtpFileProxy::copyFileFromMtpAsync(const QSharedPointer<MtpFileProxy::ProxyInfo> &proxyPtr)
{
#ifdef USE_DFM_IO
    proxyPtr->fileState = Loading;
    // dfm-io (gio)
    DOperator copyOpt(QUrl::fromLocalFile(proxyPtr->originFileName));
    copyOpt.copyFileAsync(QUrl::fromLocalFile(proxyPtr->proxyFileName),
                          DFile::CopyFlag::kOverwrite,
                          nullptr,
                          nullptr,
                          0,
                          fileOperateCallbackFunc,
                          reinterpret_cast<void *>(&proxyPtr->proxyFileName));

#else
    // 使用 Qt Api
    QFile copyFile(proxyPtr->originFileName);
    // 移除原有代理文件 QFile::copy() 默认不会覆盖文件
    if (QFile::exists(proxyPtr->proxyFileName)) {
        QFile::remove(proxyPtr->proxyFileName);
    }

    bool ret = copyFile.copy(proxyPtr->proxyFileName);
    if (!ret) {
        qWarning() << "Failed to copy MTP file using QFile. Error:" << copyFile.errorString();
    }

    loadFinished(proxyPtr->proxyFileName, ret);
#endif
}
