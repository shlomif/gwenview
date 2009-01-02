/*
Gwenview: an image viewer
Copyright 2007 Aurélien Gâteau <aurelien.gateau@free.fr>

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#ifndef ABSTRACTCONTEXTMANAGERITEM_H
#define ABSTRACTCONTEXTMANAGERITEM_H

// Qt
#include <QObject>


namespace Gwenview {

class ContextManager;
class SideBar;

struct AbstractContextManagerItemPrivate;
class AbstractContextManagerItem : public QObject {
	Q_OBJECT
public:
	AbstractContextManagerItem(ContextManager*);
	virtual ~AbstractContextManagerItem();

	virtual void setSideBar(SideBar*) = 0;

	ContextManager* contextManager() const;

private:
	AbstractContextManagerItemPrivate * const d;
};

} // namespace

#endif /* ABSTRACTCONTEXTMANAGERITEM_H */
