/*
    Pirate Bulletin Board System
    Copyright (C) 1990, Edward Luke, lush@Athena.EE.MsState.EDU
    Eagles Bulletin Board System
    Copyright (C) 1992, Raymond Rocker, rocker@rock.b11.ingr.com
                        Guy Vega, gtvega@seabass.st.usm.edu
                        Dominic Tynes, dbtynes@seabass.st.usm.edu
    Firebird Bulletin Board System
    Copyright (C) 1996, Hsien-Tsung Chang, Smallpig.bbs@bbs.cs.ccu.edu.tw
                        Peng Piaw Foong, ppfoong@csie.ncu.edu.tw

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 1, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
/*
$Id: delete.c,v 1.1 2000/01/15 01:45:28 edwardc Exp $
*/

#include "bbs.h"
#ifdef WITHOUT_ADMIN_TOOLS
#define kick_user
#endif

int
offline()
{
	char    buf[STRLEN];
	modify_user_mode(OFFLINE);
	clear();
	if (HAS_PERM(PERM_SYSOP) || HAS_PERM(PERM_BOARDS) || HAS_PERM(PERM_ADMINMENU)
		|| HAS_PERM(PERM_SEEULEVELS)) {
		move(1, 0);
		prints("\n\n您有重任在身, 不能隨便自殺啦!!\n");
		pressreturn();
		clear();
		return;
	}
	if (currentuser.stay < 86400) {
		move(1, 0);
		prints("\n\n對不起, 您還未夠資格執行此命令!!\n");
		prints("請 mail 給 SYSOP 說明自殺原因, 謝謝。\n");
		pressreturn();
		clear();
		return;
	}
	getdata(1, 0, "請輸入你的密碼: ", buf, PASSLEN, NOECHO, YEA);
	if (*buf == '\0' || !checkpasswd(currentuser.passwd, buf)) {
		prints("\n\n很抱歉, 您輸入的密碼不正確。\n");
		pressreturn();
		clear();
		return;
	}
	getdata(3, 0, "請問你叫什麼名字? ", buf, NAMELEN, DOECHO, YEA);
	if (*buf == '\0' || strcmp(buf, currentuser.realname)) {
		prints("\n\n很抱歉, 我並不認識你。\n");
		pressreturn();
		clear();
		return;
	}
	clear();
	move(1, 0);
	prints("[1;5;31m警告[0;1;31m： 自殺後, 您將無法再用此帳號進入本站！！");
	prints("\n\n\n[1;32m但帳號要在 30 天後才會刪除。好難過喔 :( .....[m\n\n\n");
	if (askyn("你確定要離開這個大家庭", NA, NA) == 1) {
		clear();
		currentuser.userlevel = 0;
		substitute_record(PASSFILE, &currentuser, sizeof(struct userec), usernum);
		mail_info();
		modify_user_mode(OFFLINE);
		kick_user(&uinfo);
		exit(0);
	}
}
getuinfo(fn)
FILE   *fn;
{
	int     num;
	char    buf[40];
	fprintf(fn, "\n\n他的代號     : %s\n", currentuser.userid);
	fprintf(fn, "他的暱稱     : %s\n", currentuser.username);
	fprintf(fn, "真實姓名     : %s\n", currentuser.realname);
	fprintf(fn, "居住住址     : %s\n", currentuser.address);
	fprintf(fn, "電子郵件信箱 : %s\n", currentuser.email);
	fprintf(fn, "真實 E-mail  : %s\n", currentuser.reginfo);
	fprintf(fn, "Ident 資料   : %s\n", currentuser.ident);
	fprintf(fn, "帳號建立日期 : %s", ctime(&currentuser.firstlogin));
	fprintf(fn, "最近光臨日期 : %s", ctime(&currentuser.lastlogin));
	fprintf(fn, "最近光臨機器 : %s\n", currentuser.lasthost);
	fprintf(fn, "上站次數     : %d 次\n", currentuser.numlogins);
	fprintf(fn, "文章數目     : %d\n", currentuser.numposts);
	fprintf(fn, "上站總時數   : %d 小時 %d 分鐘\n",
		currentuser.stay / 3600, (currentuser.stay / 60) % 60);
	strcpy(buf, "bTCPRp#@XWBA#VS-DOM-F012345678");
	for (num = 0; num < 30; num++)
		if (!(currentuser.userlevel & (1 << num)))
			buf[num] = '-';
	buf[num] = '\0';
	fprintf(fn, "使用者權限   : %s\n\n", buf);
}
mail_info()
{
	FILE   *fn;
	time_t  now;
	char    filename[STRLEN];
	now = time(0);
	sprintf(filename, "tmp/suicide.%s", currentuser.userid);
	if ((fn = fopen(filename, "w")) != NULL) {
		fprintf(fn, "[1m%s[m 已經在 [1m%24.24s[m 登記自殺了，以下是他的資料，請保留...", currentuser.userid
			,ctime(&now));
		getuinfo(fn);
		fclose(fn);
		postfile(filename, "syssecurity", "登記自殺通知(30天後生效)...", 2);
		unlink(filename);
	}
	if ((fn = fopen(filename, "w")) != NULL) {
		fprintf(fn, "大家好,\n\n");
		fprintf(fn, "我是 %s (%s)。我己經登記在30天後離開這裡了。\n\n",
			currentuser.userid, currentuser.username);
		fprintf(fn, "我不會更不可能忘記自 %s以來在本站 %d 次 login 中總共 %d 分鐘逗留期間的點點滴滴。\n",
			ctime(&currentuser.firstlogin), currentuser.numlogins, currentuser.stay / 60);
		fprintf(fn, "請我的好友把 %s 從你們的好友名單中拿掉吧。因為我己經沒有權限再上站了!\n\n",
			currentuser.userid);
		fprintf(fn, "或許\有朝一日我會回來的。 珍重!! 再見!!\n\n\n");
		fprintf(fn, "%s 於 %24.24s 留.\n\n", currentuser.userid, ctime(&now));
		fclose(fn);
		postfile(filename, "notepad", "登記自殺留言...", 2);
		unlink(filename);
	}
}
