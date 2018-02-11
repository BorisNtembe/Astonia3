/*
 *
 * $Id: transport.c,v 1.8 2008/04/11 10:53:43 devel Exp $ (c) 2005 D.Brockhaus
 *
 * $Log: transport.c,v $
 * Revision 1.8  2008/04/11 10:53:43  devel
 * added arkhata teleport
 *
 * Revision 1.7  2007/06/09 15:47:11  devel
 * added aston2 and caligar transports
 *
 * Revision 1.6  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 * Revision 1.5  2006/06/25 10:38:06  ssim
 * added transport in teufelheim
 *
 * Revision 1.4  2005/12/04 17:17:44  ssim
 * removed need for key for grimroot teleport
 *
 * Revision 1.3  2005/12/04 11:26:01  ssim
 * added teleport in nomad plains
 *
 * Revision 1.2  2005/12/04 10:35:15  ssim
 * removed unused ice teleports
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "server.h"
#include "log.h"
#include "notify.h"
#include "direction.h"
#include "do.h"
#include "path.h"
#include "error.h"
#include "drdata.h"
#include "see.h"
#include "death.h"
#include "talk.h"
#include "effect.h"
#include "database.h"
#include "map.h"
#include "create.h"
#include "container.h"
#include "drvlib.h"
#include "tool.h"
#include "spell.h"
#include "effect.h"
#include "light.h"
#include "date.h"
#include "los.h"
#include "item_id.h"
#include "clan.h"
#include "player.h"
#include "client.h"
#include "io.h"
#include "transport.h"

// library helper functions needed for init
int ch_driver(int nr,int cn,int ret,int lastact);	// character driver (decides next action)
int it_driver(int nr,int in,int cn);					// item driver (special cases for use)
int ch_died_driver(int nr,int cn,int co);				// called when a character dies

// EXPORTED - character/item driver
int driver(int type,int nr,int obj,int ret,int lastact)
{
	switch(type) {
		case 0:		return ch_driver(nr,obj,ret,lastact);
		case 1: 	return it_driver(nr,obj,ret);
		case 2:		return ch_died_driver(nr,obj,ret);
		default: 	return 0;
	}
}

struct trans
{
	char name[80];
	int x,y,a;
};

static struct trans trans[]=
{
	{"Cameron",139,75,1},				//00
	{"Cameron",139,75,1},				//01
	{"Aston",129,201,3},				//02
	{"Tribe of the Isara",239,249,6},		//03
	{"Tribe of the Cerasa",92,164,6},		//04
	{"Maze of the Cerasa",49,135,6},		//05
	{"Defense Tunnels of the Cerasa",14,114,6},	//06
	{"Zalina Entrance",5,4,6},			//07
	{"Tribe of the Zalina",172,36,6},		//08
	{"*empty*",0,0,0},				//09
	{"Aston 2",230,96,3},				//10
	{"*empty*",0,0,0},				//11
	{"Ice 1",93,102,10},				//12
	{"Ice 2",11,113,10},				//13
	{"Ice 3",241,87,10},				//14
	{"Ice 4",213,156,11},				//15
	{"Ice 5",189,80,11},				//16
	{"Nomad Plains",16,124,19},			//17
	{"*empty*",0,0,0},				//18
	{"*empty*",0,0,0},				//19
	{"Forest",181,117,16},				//20
	{"Exkordon",65,106,17},				//21
	{"Brannington",202,226,29},			//22
	{"Grimroot",210,246,31},			//23
        {"Caligar",230,62,36},                          //24
	{"Arkhata",28,20,37},				//25
};

void transport_driver(int in,int cn)
{
	int x,y,a,oldx,oldy,nr,mirror,oldmirror;
	unsigned long long bit;
	struct transport_ppd *dat;
	unsigned char buf[16];

        if (!cn) return;	// always make sure its not an automatic call if you don't handle it

	dat=set_data(cn,DRD_TRANSPORT_PPD,sizeof(struct transport_ppd));
	if (!dat) return;	// oops...

	// mark new transports as seen
        nr=it[in].drdata[0];
	if (nr!=255) {	// not clan exit
		if (nr<0 || nr>=ARRAYSIZE(trans)) {
			elog("illegal transport nr %d from item %d (%s) #1",nr,in,it[in].name);
			log_char(cn,LOG_SYSTEM,0,"Nothing happens - BUG (%d,#1).",nr);
			return;
		}
	
		bit=1<<nr;
		if (!(dat->seen&bit)) {
			log_char(cn,LOG_SYSTEM,0,"You have reached a new transportation point.");
			dat->seen|=bit;
		}
	}

	if (ch[cn].act2==0) {
		buf[0]=SV_TELEPORT;

		*(unsigned long long*)(buf+1)=dat->seen;
                psend(ch[cn].player,buf,9);

		return;
	}

        nr=(ch[cn].act2&255)-1;
        mirror=(ch[cn].act2/256);
        if (mirror<1 || mirror>26) mirror=RANDOM(26)+1;

	// clan hall is target
	if (nr>63 && nr<64+60) {
		if (may_enter_clan(cn,nr-63)) {
			x=17+((nr-64)/12)*49;
			y=16+((nr-64)%12)*20;
			a=13;
		} else {
			x=43+((nr-64)/12)*49;
			y=15+((nr-64)%12)*20;
			a=13;
		}
	} else if (nr<64) {
		bit=1<<nr;
		if (!(dat->seen&bit)) {
			log_char(cn,LOG_SYSTEM,0,"You've never been to %s before. You cannot go there.",trans[nr].name);
			return;
		}

		if (nr==22 && !(ch[cn].flags&CF_ARCH)) {
			log_char(cn,LOG_SYSTEM,0,"Sorry, Arches only!");
			return;
		}

                if (nr<0 || nr>=ARRAYSIZE(trans)) {
			//elog("illegal transport nr %d #2",nr);
			log_char(cn,LOG_SYSTEM,0,"Nothing happens - BUG (%d,#2).",nr);
			return;
		}
	
		x=trans[nr].x;
		y=trans[nr].y;
		a=trans[nr].a;
	} else {
		log_char(cn,LOG_SYSTEM,0,"You've confused me. (BUG #1123)");
		return;
	}
	
        if (x<1 || x>MAXMAP-2 || y<1 || y>MAXMAP-2) {
		log_char(cn,LOG_SYSTEM,0,"Nothing happens - BUG (%d,%d,%d).",x,y,a);
		return;
	}

	oldmirror=ch[cn].mirror;
	ch[cn].mirror=mirror;
        if ((ch[cn].flags&CF_PLAYER) && a && (a!=areaID || get_mirror(a,ch[cn].mirror)!=areaM)) {
		if (!change_area(cn,a,x,y)) {
			log_char(cn,LOG_SYSTEM,0,"Nothing happens - target area server is down.");
			ch[cn].mirror=oldmirror;
		}
                return;
	}
	buf[0]=SV_MIRROR;
	*(unsigned int*)(buf+1)=ch[cn].mirror;
	psend(ch[cn].player,buf,5);	

        oldx=ch[cn].x; oldy=ch[cn].y;
	remove_char(cn);
	
	if (!drop_char(cn,x,y,0)) {
		log_char(cn,LOG_SYSTEM,0,"Please try again soon. Target is busy");
		drop_char(cn,oldx,oldy,0);
	}
}

int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
                default:	return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_TRANSPORT:	transport_driver(in,cn); return 1;
		
		default:	return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
                default:	return 0;
	}
}
