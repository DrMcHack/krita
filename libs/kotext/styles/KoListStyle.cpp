/* This file is part of the KDE project
 * Copyright (C) 2006-2007 Thomas Zander <zander@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "KoListStyle.h"
#include "KoListLevelProperties.h"
#include "KoTextBlockData.h"

#include <KoStyleStack.h>
#include <KoOasisStyles.h>
#include <KoOasisLoadingContext.h>
#include <KoXmlNS.h>
#include <kdebug.h>
#include <QTextCursor>

class KoListStyle::Private {
public:
    Private() : refCount(1) { }

    QTextList *textList(int level, const QTextDocument *doc) {
        if(! textLists.contains(level))
            return 0;
        QMap<const QTextDocument*, QPointer<QTextList> > map = textLists[level];
        if(! map.contains(doc))
            return 0;
        QPointer<QTextList> pointer = map[doc];
        if(pointer.isNull())
            return 0;
        return pointer;
    }

    void setTextList(int level, const QTextDocument *doc, QTextList *list) {
        QMap<const QTextDocument*, QPointer<QTextList> > map = textLists[level];
        map.insert(doc, QPointer<QTextList>(list));
        textLists.insert(level, map);
    }

    QString name;
    QMap<int, KoListLevelProperties> levels;
    QMap<int, QMap<const QTextDocument*, QPointer<QTextList> > > textLists;
    int refCount;
};

KoListStyle::KoListStyle()
    : d( new Private())
{
}

KoListStyle::KoListStyle(const KoListStyle &orig)
    : d( orig.d )
{
    d->refCount++;
}

KoListStyle::KoListStyle(int)
    : d(0)
{
}

KoListStyle::~KoListStyle() {
    if(d && --d->refCount == 0)
        delete d;
}

bool KoListStyle::operator==(const KoListStyle &other) const {
    foreach(int level, d->levels.keys()) {
        if(! other.hasPropertiesForLevel(level))
            return false;
        if(!(other.level(level) == d->levels[level]))
            return false;
    }
    foreach(int level, other.d->levels.keys()) {
        if(! hasPropertiesForLevel(level))
            return false;
    }
    return true;
}

QString KoListStyle::name() const {
    return d->name;
}

void KoListStyle::setName(const QString &name) {
    d->name = name;
}

KoListLevelProperties KoListStyle::level(int level) const {
    if(d->levels.contains(level))
        return d->levels.value(level);
    if(d->levels.count()) {
        KoListLevelProperties llp = d->levels.begin().value();
        llp.setLevel(level);
        return llp;
    }
    KoListLevelProperties llp;
    llp.setLevel(level);
    return llp;
}

void KoListStyle::setLevel(const KoListLevelProperties &properties) {
    d->levels.insert(properties.level(), properties);

    // find all QTextList objects and apply the changed style on them.
    if(! d->textLists.contains(properties.level()))
        return;
    QMap<const QTextDocument*, QPointer<QTextList> > map = d->textLists.value(properties.level());
    foreach(QPointer<QTextList> list, map) {
        if(list.isNull()) continue;
        QTextListFormat format = list->format();
        properties.applyStyle(format);
        list->setFormat(format);

        QTextBlock tb = list->item(0);
        if(tb.isValid()) { // invalidate the counter part
            KoTextBlockData *userData = dynamic_cast<KoTextBlockData*> (tb.userData());
            if(userData)
                userData->setCounterWidth(-1.0);
        }
    }
}

bool KoListStyle::hasPropertiesForLevel(int level) const {
    return d->levels.contains(level);
}

void KoListStyle::removePropertiesForLevel(int level) {
    d->levels.remove(level);
}

void KoListStyle::applyStyle(const QTextBlock &block, int level) {
    if(level == 0) { // illegal level; fetch the first proper level we have
        if(d->levels.count())
            level = d->levels.keys().first();
        else // just go for default, then
            level = 1;
    }

    const bool contains = hasPropertiesForLevel(level);

    QTextList *textList = d->textList(level, block.document());
    if(textList && block.textList() && block.textList() != textList) // remove old one
        block.textList()->remove(block);
    if(block.textList() == 0 && textList) // add if new
        textList->add(block);
    if(block.textList() && textList == 0) {
        textList = block.textList(); // missed it ?
        d->setTextList(level, block.document(), textList);
    }

    QTextListFormat format;
    if(block.textList())
        format = block.textList()->format();

    KoListLevelProperties llp = this->level(level);
    llp.applyStyle(format);

    if(textList) {
        textList->setFormat(format);
        QTextBlock tb = textList->item(0);
        if(tb.isValid()) { // invalidate the counter part
            KoTextBlockData *userData = dynamic_cast<KoTextBlockData*> (tb.userData());
            if(userData)
                userData->setCounterWidth(-1.0);
        }
    }
    else { // does not exist yet, this is the first parag that uses it :)
        QTextCursor cursor(block);
        textList = cursor.createList(format);
        d->setTextList(level, block.document(), textList);
    }

    if(contains)
        d->levels.insert(level, llp);
}

bool KoListStyle::isValid() const {
    return d != 0;
}

// static
KoListStyle* KoListStyle::fromTextList(QTextList *list) {
    KoListStyle *answer = new KoListStyle();
    KoListLevelProperties llp = KoListLevelProperties::fromTextList(list);
    answer->setLevel(llp);
    answer->d->setTextList(llp.level(), list->document(), list);
    return answer;
}

void KoListStyle::loadOasis(KoOasisLoadingContext& context)
{
    //kDebug()<<"KoListStyle::loadOasis"<<endl;
    //KoStyleStack &styleStack = context.styleStack();

    KoListLevelProperties properties;

    KoXmlElement* listElem = context.oasisStyles().listStyles()[ name() ];
    Style style = KoListStyle::NoItem;
    if( listElem ) {
        KoXmlElement listStyle = listElem->firstChildElement("list-level-style-bullet");
        if( ! listStyle.isNull() ) {
            //e.attributeNS(KoXmlNS::text, "level", QString::null);

            /*
            QString prefix = listStyle.attributeNS(KoXmlNS::style, "num-prefix", QString::null);
            if( ! prefix.isNull() )
                properties.setListItemPrefix(prefix);
            QString suffix = listStyle.attributeNS(KoXmlNS::style, "num-suffix", QString::null);
            if( ! suffix.isNull() )
                properties.setListItemSuffix(suffix);
            */

            //1.6: KoParagCounter::loadOasisListStyle
            QString bulletChar = listStyle.attributeNS( KoXmlNS::text, "bullet-char", QString::null );
            if( ! bulletChar.isEmpty() ) {
                // Reverse engineering, I found those codes:
                switch( bulletChar[0].unicode() ) {
                    case 0x2022: // small disc -> circle
                        style = KoListStyle::CircleItem;
                        break;
                    case 0x25CF: // large disc -> disc
                    case 0xF0B7: // #113361
                        style = KoListStyle::DiscItem;
                        break;
                    case 0xE00C: // losange - TODO in kotext. Not in OASIS either (reserved Unicode area!)
                        style = KoListStyle::SquareItem;
                        break;
                    case 0xE00A: // square. Not in OASIS (reserved Unicode area!), but used in both OOo and kotext.
                        style = KoListStyle::SquareItem;
                        break;
                    case 0x27A2: // two-colors right-pointing triangle
                                // mapping (both ways) to box for now.
                        style = KoListStyle::BoxItem;
                        break;

                    case 0x2794: // arrow to right
                        style = KoListStyle::CustomCharItem;
                        properties.setBulletCharacter('>');
                        break;
                    case 0x2714: // checkmark
                        style = KoListStyle::CustomCharItem;
                        properties.setBulletCharacter('v');
                        break;
                    case 0x2d: // minus
                        style = KoListStyle::CustomCharItem;
                        properties.setBulletCharacter('-');
                        break;
                    case 0x2717: // cross
                        style = KoListStyle::CustomCharItem;
                        properties.setBulletCharacter('x');
                        break;
                    default:
                        QChar customBulletChar = bulletChar[0];
                        kDebug() << "Unhandled bullet code 0x" << QString::number( (uint)customBulletChar.unicode(), 16 ) << endl;
                        /*
                        QString customBulletFont;
                        // often StarSymbol when it comes from OO; doesn't matter, Qt finds it in another font if needed.
                        if ( listStyleProperties.hasAttributeNS( KoXmlNS::style, "font-name" ) )
                        {
                            customBulletFont = listStyleProperties.attributeNS( KoXmlNS::style, "font-name", QString::null );
                            kDebug() << "customBulletFont style:font-name = " << listStyleProperties.attributeNS( KoXmlNS::style, "font-name", QString::null ) << endl;
                        }
                        else if ( listStyleTextProperties.hasAttributeNS( KoXmlNS::fo, "font-family" ) )
                        {
                            customBulletFont = listStyleTextProperties.attributeNS( KoXmlNS::fo, "font-family", QString::null );
                            kDebug() << "customBulletFont fo:font-family = " << listStyleTextProperties.attributeNS( KoXmlNS::fo, "font-family", QString::null ) << endl;
                        }
                        // ## TODO in fact we're supposed to read it from the style pointed to by text:style-name
                        */
                        break;
                }
            }
        }
        else {
            style = KoListStyle::DecimalItem;
        }
    }

    properties.setStyle(style);
    properties.setLevel(0);
    setLevel(properties);
    //liststyle->setLevel( element.attribute("depth").toInt() + 1);
}
