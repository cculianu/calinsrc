/****************************************************************************
** ECGGraph meta object code from reading C++ file 'ecggraph.h'
**
** Created: Mon Aug 20 17:38:52 2001
**      by: The Qt MOC ($Id$)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#if !defined(Q_MOC_OUTPUT_REVISION)
#define Q_MOC_OUTPUT_REVISION 9
#elif Q_MOC_OUTPUT_REVISION != 9
#error "Moc format conflict - please regenerate all moc files"
#endif

#include "ecggraph.h"
#include <qmetaobject.h>
#include <qapplication.h>



const char *ECGGraph::className() const
{
    return "ECGGraph";
}

QMetaObject *ECGGraph::metaObj = 0;

void ECGGraph::initMetaObject()
{
    if ( metaObj )
	return;
    if ( qstrcmp(QWidget::className(), "QWidget") != 0 )
	badSuperclassWarning("ECGGraph","QWidget");
    (void) staticMetaObject();
}

#ifndef QT_NO_TRANSLATION

QString ECGGraph::tr(const char* s)
{
    return qApp->translate( "ECGGraph", s, 0 );
}

QString ECGGraph::tr(const char* s, const char * c)
{
    return qApp->translate( "ECGGraph", s, c );
}

#endif // QT_NO_TRANSLATION

QMetaObject* ECGGraph::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    (void) QWidget::staticMetaObject();
#ifndef QT_NO_PROPERTIES
#endif // QT_NO_PROPERTIES
    typedef void (ECGGraph::*m1_t0)(double,double);
    typedef void (QObject::*om1_t0)(double,double);
    typedef void (ECGGraph::*m1_t1)(double);
    typedef void (QObject::*om1_t1)(double);
    typedef void (ECGGraph::*m1_t2)();
    typedef void (QObject::*om1_t2)();
    m1_t0 v1_0 = &ECGGraph::setRange;
    om1_t0 ov1_0 = (om1_t0)v1_0;
    m1_t1 v1_1 = &ECGGraph::setSpikeThreshHold;
    om1_t1 ov1_1 = (om1_t1)v1_1;
    m1_t2 v1_2 = &ECGGraph::unsetSpikeThreshHold;
    om1_t2 ov1_2 = (om1_t2)v1_2;
    QMetaData *slot_tbl = QMetaObject::new_metadata(3);
    QMetaData::Access *slot_tbl_access = QMetaObject::new_metaaccess(3);
    slot_tbl[0].name = "setRange(double,double)";
    slot_tbl[0].ptr = (QMember)ov1_0;
    slot_tbl_access[0] = QMetaData::Public;
    slot_tbl[1].name = "setSpikeThreshHold(double)";
    slot_tbl[1].ptr = (QMember)ov1_1;
    slot_tbl_access[1] = QMetaData::Public;
    slot_tbl[2].name = "unsetSpikeThreshHold()";
    slot_tbl[2].ptr = (QMember)ov1_2;
    slot_tbl_access[2] = QMetaData::Public;
    typedef void (ECGGraph::*m2_t0)(double,double);
    typedef void (QObject::*om2_t0)(double,double);
    typedef void (ECGGraph::*m2_t1)(const ECGGraph*,long long,double);
    typedef void (QObject::*om2_t1)(const ECGGraph*,long long,double);
    typedef void (ECGGraph::*m2_t2)(double);
    typedef void (QObject::*om2_t2)(double);
    typedef void (ECGGraph::*m2_t3)(long long);
    typedef void (QObject::*om2_t3)(long long);
    typedef void (ECGGraph::*m2_t4)(double,long long);
    typedef void (QObject::*om2_t4)(double,long long);
    typedef void (ECGGraph::*m2_t5)();
    typedef void (QObject::*om2_t5)();
    typedef void (ECGGraph::*m2_t6)();
    typedef void (QObject::*om2_t6)();
    m2_t0 v2_0 = &ECGGraph::rangeChanged;
    om2_t0 ov2_0 = (om2_t0)v2_0;
    m2_t1 v2_1 = &ECGGraph::spikeDetected;
    om2_t1 ov2_1 = (om2_t1)v2_1;
    m2_t2 v2_2 = &ECGGraph::mouseOverAmplitude;
    om2_t2 ov2_2 = (om2_t2)v2_2;
    m2_t3 v2_3 = &ECGGraph::mouseOverSampleIndex;
    om2_t3 ov2_3 = (om2_t3)v2_3;
    m2_t4 v2_4 = &ECGGraph::mouseOverVector;
    om2_t4 ov2_4 = (om2_t4)v2_4;
    m2_t5 v2_5 = &ECGGraph::rightClicked;
    om2_t5 ov2_5 = (om2_t5)v2_5;
    m2_t6 v2_6 = &ECGGraph::leftClicked;
    om2_t6 ov2_6 = (om2_t6)v2_6;
    QMetaData *signal_tbl = QMetaObject::new_metadata(7);
    signal_tbl[0].name = "rangeChanged(double,double)";
    signal_tbl[0].ptr = (QMember)ov2_0;
    signal_tbl[1].name = "spikeDetected(const ECGGraph*,long long,double)";
    signal_tbl[1].ptr = (QMember)ov2_1;
    signal_tbl[2].name = "mouseOverAmplitude(double)";
    signal_tbl[2].ptr = (QMember)ov2_2;
    signal_tbl[3].name = "mouseOverSampleIndex(long long)";
    signal_tbl[3].ptr = (QMember)ov2_3;
    signal_tbl[4].name = "mouseOverVector(double,long long)";
    signal_tbl[4].ptr = (QMember)ov2_4;
    signal_tbl[5].name = "rightClicked()";
    signal_tbl[5].ptr = (QMember)ov2_5;
    signal_tbl[6].name = "leftClicked()";
    signal_tbl[6].ptr = (QMember)ov2_6;
    metaObj = QMetaObject::new_metaobject(
	"ECGGraph", "QWidget",
	slot_tbl, 3,
	signal_tbl, 7,
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

#include <qobjectdefs.h>
#include <qsignalslotimp.h>

// SIGNAL rangeChanged
void ECGGraph::rangeChanged( double t0, double t1 )
{
    // No builtin function for signal parameter type double,double
    QConnectionList *clist = receivers("rangeChanged(double,double)");
    if ( !clist || signalsBlocked() )
	return;
    typedef void (QObject::*RT0)();
    typedef void (QObject::*RT1)(double);
    typedef void (QObject::*RT2)(double,double);
    RT0 r0;
    RT1 r1;
    RT2 r2;
    QConnectionListIt it(*clist);
    QConnection   *c;
    QSenderObject *object;
    while ( (c=it.current()) ) {
	++it;
	object = (QSenderObject*)c->object();
	object->setSender( this );
	switch ( c->numArgs() ) {
	    case 0:
#ifdef Q_FP_CCAST_BROKEN
		r0 = reinterpret_cast<RT0>(*(c->member()));
#else
		r0 = (RT0)*(c->member());
#endif
		(object->*r0)();
		break;
	    case 1:
#ifdef Q_FP_CCAST_BROKEN
		r1 = reinterpret_cast<RT1>(*(c->member()));
#else
		r1 = (RT1)*(c->member());
#endif
		(object->*r1)(t0);
		break;
	    case 2:
#ifdef Q_FP_CCAST_BROKEN
		r2 = reinterpret_cast<RT2>(*(c->member()));
#else
		r2 = (RT2)*(c->member());
#endif
		(object->*r2)(t0, t1);
		break;
	}
    }
}

// SIGNAL spikeDetected
void ECGGraph::spikeDetected( const ECGGraph* t0, long long t1, double t2 )
{
    // No builtin function for signal parameter type const ECGGraph*,long long,double
    QConnectionList *clist = receivers("spikeDetected(const ECGGraph*,long long,double)");
    if ( !clist || signalsBlocked() )
	return;
    typedef void (QObject::*RT0)();
    typedef void (QObject::*RT1)(const ECGGraph*);
    typedef void (QObject::*RT2)(const ECGGraph*,long long);
    typedef void (QObject::*RT3)(const ECGGraph*,long long,double);
    RT0 r0;
    RT1 r1;
    RT2 r2;
    RT3 r3;
    QConnectionListIt it(*clist);
    QConnection   *c;
    QSenderObject *object;
    while ( (c=it.current()) ) {
	++it;
	object = (QSenderObject*)c->object();
	object->setSender( this );
	switch ( c->numArgs() ) {
	    case 0:
#ifdef Q_FP_CCAST_BROKEN
		r0 = reinterpret_cast<RT0>(*(c->member()));
#else
		r0 = (RT0)*(c->member());
#endif
		(object->*r0)();
		break;
	    case 1:
#ifdef Q_FP_CCAST_BROKEN
		r1 = reinterpret_cast<RT1>(*(c->member()));
#else
		r1 = (RT1)*(c->member());
#endif
		(object->*r1)(t0);
		break;
	    case 2:
#ifdef Q_FP_CCAST_BROKEN
		r2 = reinterpret_cast<RT2>(*(c->member()));
#else
		r2 = (RT2)*(c->member());
#endif
		(object->*r2)(t0, t1);
		break;
	    case 3:
#ifdef Q_FP_CCAST_BROKEN
		r3 = reinterpret_cast<RT3>(*(c->member()));
#else
		r3 = (RT3)*(c->member());
#endif
		(object->*r3)(t0, t1, t2);
		break;
	}
    }
}

// SIGNAL mouseOverAmplitude
void ECGGraph::mouseOverAmplitude( double t0 )
{
    // No builtin function for signal parameter type double
    QConnectionList *clist = receivers("mouseOverAmplitude(double)");
    if ( !clist || signalsBlocked() )
	return;
    typedef void (QObject::*RT0)();
    typedef void (QObject::*RT1)(double);
    RT0 r0;
    RT1 r1;
    QConnectionListIt it(*clist);
    QConnection   *c;
    QSenderObject *object;
    while ( (c=it.current()) ) {
	++it;
	object = (QSenderObject*)c->object();
	object->setSender( this );
	switch ( c->numArgs() ) {
	    case 0:
#ifdef Q_FP_CCAST_BROKEN
		r0 = reinterpret_cast<RT0>(*(c->member()));
#else
		r0 = (RT0)*(c->member());
#endif
		(object->*r0)();
		break;
	    case 1:
#ifdef Q_FP_CCAST_BROKEN
		r1 = reinterpret_cast<RT1>(*(c->member()));
#else
		r1 = (RT1)*(c->member());
#endif
		(object->*r1)(t0);
		break;
	}
    }
}

// SIGNAL mouseOverSampleIndex
void ECGGraph::mouseOverSampleIndex( long long t0 )
{
    // No builtin function for signal parameter type long long
    QConnectionList *clist = receivers("mouseOverSampleIndex(long long)");
    if ( !clist || signalsBlocked() )
	return;
    typedef void (QObject::*RT0)();
    typedef void (QObject::*RT1)(long long);
    RT0 r0;
    RT1 r1;
    QConnectionListIt it(*clist);
    QConnection   *c;
    QSenderObject *object;
    while ( (c=it.current()) ) {
	++it;
	object = (QSenderObject*)c->object();
	object->setSender( this );
	switch ( c->numArgs() ) {
	    case 0:
#ifdef Q_FP_CCAST_BROKEN
		r0 = reinterpret_cast<RT0>(*(c->member()));
#else
		r0 = (RT0)*(c->member());
#endif
		(object->*r0)();
		break;
	    case 1:
#ifdef Q_FP_CCAST_BROKEN
		r1 = reinterpret_cast<RT1>(*(c->member()));
#else
		r1 = (RT1)*(c->member());
#endif
		(object->*r1)(t0);
		break;
	}
    }
}

// SIGNAL mouseOverVector
void ECGGraph::mouseOverVector( double t0, long long t1 )
{
    // No builtin function for signal parameter type double,long long
    QConnectionList *clist = receivers("mouseOverVector(double,long long)");
    if ( !clist || signalsBlocked() )
	return;
    typedef void (QObject::*RT0)();
    typedef void (QObject::*RT1)(double);
    typedef void (QObject::*RT2)(double,long long);
    RT0 r0;
    RT1 r1;
    RT2 r2;
    QConnectionListIt it(*clist);
    QConnection   *c;
    QSenderObject *object;
    while ( (c=it.current()) ) {
	++it;
	object = (QSenderObject*)c->object();
	object->setSender( this );
	switch ( c->numArgs() ) {
	    case 0:
#ifdef Q_FP_CCAST_BROKEN
		r0 = reinterpret_cast<RT0>(*(c->member()));
#else
		r0 = (RT0)*(c->member());
#endif
		(object->*r0)();
		break;
	    case 1:
#ifdef Q_FP_CCAST_BROKEN
		r1 = reinterpret_cast<RT1>(*(c->member()));
#else
		r1 = (RT1)*(c->member());
#endif
		(object->*r1)(t0);
		break;
	    case 2:
#ifdef Q_FP_CCAST_BROKEN
		r2 = reinterpret_cast<RT2>(*(c->member()));
#else
		r2 = (RT2)*(c->member());
#endif
		(object->*r2)(t0, t1);
		break;
	}
    }
}

// SIGNAL rightClicked
void ECGGraph::rightClicked()
{
    activate_signal( "rightClicked()" );
}

// SIGNAL leftClicked
void ECGGraph::leftClicked()
{
    activate_signal( "leftClicked()" );
}
