/****************************************************************************
** ECGGraphContainer meta object code from reading C++ file 'ecggraphcontainer.h'
**
** Created: Tue Aug 14 11:42:46 2001
**      by: The Qt MOC ($Id$)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#if !defined(Q_MOC_OUTPUT_REVISION)
#define Q_MOC_OUTPUT_REVISION 9
#elif Q_MOC_OUTPUT_REVISION != 9
#error "Moc format conflict - please regenerate all moc files"
#endif

#include "ecggraphcontainer.h"
#include <qmetaobject.h>
#include <qapplication.h>



const char *ECGGraphContainer::className() const
{
    return "ECGGraphContainer";
}

QMetaObject *ECGGraphContainer::metaObj = 0;

void ECGGraphContainer::initMetaObject()
{
    if ( metaObj )
	return;
    if ( qstrcmp(QFrame::className(), "QFrame") != 0 )
	badSuperclassWarning("ECGGraphContainer","QFrame");
    (void) staticMetaObject();
}

#ifndef QT_NO_TRANSLATION

QString ECGGraphContainer::tr(const char* s)
{
    return qApp->translate( "ECGGraphContainer", s, 0 );
}

QString ECGGraphContainer::tr(const char* s, const char * c)
{
    return qApp->translate( "ECGGraphContainer", s, c );
}

#endif // QT_NO_TRANSLATION

QMetaObject* ECGGraphContainer::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    (void) QFrame::staticMetaObject();
#ifndef QT_NO_PROPERTIES
#endif // QT_NO_PROPERTIES
    typedef void (ECGGraphContainer::*m1_t0)(const QString&);
    typedef void (QObject::*om1_t0)(const QString&);
    typedef void (ECGGraphContainer::*m1_t1)(double,double);
    typedef void (QObject::*om1_t1)(double,double);
    m1_t0 v1_0 = &ECGGraphContainer::rangeChange;
    om1_t0 ov1_0 = (om1_t0)v1_0;
    m1_t1 v1_1 = &ECGGraphContainer::updateYAxisLabels;
    om1_t1 ov1_1 = (om1_t1)v1_1;
    QMetaData *slot_tbl = QMetaObject::new_metadata(2);
    QMetaData::Access *slot_tbl_access = QMetaObject::new_metaaccess(2);
    slot_tbl[0].name = "rangeChange(const QString&)";
    slot_tbl[0].ptr = (QMember)ov1_0;
    slot_tbl_access[0] = QMetaData::Public;
    slot_tbl[1].name = "updateYAxisLabels(double,double)";
    slot_tbl[1].ptr = (QMember)ov1_1;
    slot_tbl_access[1] = QMetaData::Public;
    metaObj = QMetaObject::new_metaobject(
	"ECGGraphContainer", "QFrame",
	slot_tbl, 2,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    metaObj->set_slot_access( slot_tbl_access );
#ifndef QT_NO_PROPERTIES
#endif // QT_NO_PROPERTIES
    return metaObj;
}
