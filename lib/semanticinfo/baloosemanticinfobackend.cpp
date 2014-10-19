// vim: set tabstop=4 shiftwidth=4 expandtab:
/*
Gwenview: an image viewer
Copyright 2008 Aurélien Gâteau <agateau@kde.org>
Copyright 2014 Vishesh Handa <me@vhanda.in>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Cambridge, MA 02110-1301, USA.

*/
// Self
#include "baloosemanticinfobackend.h"

// Local
#include <lib/gvdebug.h>

// Qt
#include <QDebug>
#include <QUrl>

// KDE

// Baloo
#include <baloo/file.h>
//#include <baloo/filefetchjob.h>
//#include <baloo/filemodifyjob.h>
#include <baloo/taglistjob.h>

namespace Gwenview
{

struct BalooSemanticInfoBackend::Private
{
    TagSet mAllTags;
};

BalooSemanticInfoBackend::BalooSemanticInfoBackend(QObject* parent)
: AbstractSemanticInfoBackEnd(parent)
, d(new BalooSemanticInfoBackend::Private)
{
}

BalooSemanticInfoBackend::~BalooSemanticInfoBackend()
{
    delete d;
}

TagSet BalooSemanticInfoBackend::allTags() const
{
    if (d->mAllTags.empty()) {
        const_cast<BalooSemanticInfoBackend*>(this)->refreshAllTags();
    }
    return d->mAllTags;
}

void BalooSemanticInfoBackend::refreshAllTags()
{
    Baloo::TagListJob* job = new Baloo::TagListJob();
    job->exec();

    d->mAllTags.clear();
    Q_FOREACH(const QString& tag, job->tags()) {
        d->mAllTags << tag;
    }
}

void BalooSemanticInfoBackend::storeSemanticInfo(const QUrl &url, const SemanticInfo& semanticInfo)
{
    Baloo::File file(url.toLocalFile());
    //PORT BALOO file.setRating(semanticInfo.mRating);
    //PORT BALOO file.setUserComment(semanticInfo.mDescription);
    //PORT BALOO file.setTags(semanticInfo.mTags.toList());
#if 0 //PORT BALOO
    Baloo::FileModifyJob* job = new Baloo::FileModifyJob(file);
    job->start();
#endif
}

void BalooSemanticInfoBackend::retrieveSemanticInfo(const QUrl &url)
{
#if 0 //PORT BALOO
    Baloo::FileFetchJob* job = new Baloo::FileFetchJob(url.toLocalFile());
    connect(job, &Baloo::FileFetchJob::finished, this, &BalooSemanticInfoBackend::slotFetchFinished);

    job->start();
#endif
}

void BalooSemanticInfoBackend::slotFetchFinished(KJob* job)
{
#if 0 //PORT BALOO
    Baloo::FileFetchJob* fjob = static_cast<Baloo::FileFetchJob*>(job);
    Baloo::File file = fjob->file();

    SemanticInfo si;
    //PORT BALOO si.mRating = file.rating();
    //PORT BALOO si.mDescription = file.userComment();
    //PORT BALOO si.mTags = file.tags().toSet();

    //PORT BALOO emit semanticInfoRetrieved(QUrl::fromLocalFile(file.url()), si);
#endif
}

QString BalooSemanticInfoBackend::labelForTag(const SemanticInfoTag& uriString) const
{
    return uriString;
}

SemanticInfoTag BalooSemanticInfoBackend::tagForLabel(const QString& label)
{
    return label;
}

} // namespace
