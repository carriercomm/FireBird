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
$Id: register.c,v 1.14 2002/09/03 13:56:28 edwardc Exp $
*/

#include "bbs.h"
char   *sysconf_str();
char   *genpasswd();
char   *Ctime();

extern char fromhost[60];
extern time_t login_start_time;
time_t  system_time;

int
bad_user_id(userid)
char   *userid;
{
	FILE   *fp;
	char    buf[STRLEN];
	char   *ptr;
	ptr = userid;

	if ((fp = fopen(F_BAD_ID, "r")) != NULL) {
		while (fgets(buf, STRLEN, fp) != NULL) {
			ptr = strtok(buf, "\n\t\r");
			if (ptr != NULL && *ptr != '#' && strcasecmp_match(userid, ptr) == 0) {
				fclose(fp);
				return 1;
			}
		}
		fclose(fp);
	}
	return 0;
}

int
compute_user_value(urec)
struct userec *urec;
{
	int     value;
	/* if (urec) has XEMPT permission, don't kick it */
	if ((urec->userlevel & PERM_XEMPT) || strcmp(urec->userid, "guest") == 0)
		return 999;
	value = (time(0) - urec->lastlogin) / 60;	/* min */
	/* new user should register in 30 mins */
	if (strcmp(urec->userid, "new") == 0) {
		return (30 - value) * 60;
	}
	if (urec->numlogins <= 3)
		return (15 * 1440 - value) / 1440;
	if (!(urec->userlevel & PERM_LOGINOK))
		return (30 * 1440 - value) / 1440;
	if (urec->stay > 1000000)
		return (365 * 1440 - value) / 1440;
	return (120 * 1440 - value) / 1440;
}

int
getnewuserid()
{
	struct userec utmp, zerorec;
	struct stat st;
	int     fd, size, val, i;
	FILE   *fn;
	char   *ptr;
	char    buf[20];
	char    buf2[STRLEN];
	system_time = time(NULL);
	if (stat(F_KILL_USER, &st) == -1 || st.st_mtime < system_time - 3600) {
		if ((fd = open(F_KILL_USER, O_RDWR | O_CREAT, 0600)) == -1)
			return -1;
		write(fd, ctime(&system_time), 25);
		close(fd);
		log_usies("CLEAN", "dated users.");
		printf("尋找新帳號中, 請稍待片刻...\n\r");
		memset(&zerorec, 0, sizeof(zerorec));
		if ((fd = open(PASSFILE, O_RDWR | O_CREAT, 0600)) == -1)
			return -1;
		size = sizeof(utmp);
		for (i = 0; i < MAXUSERS; i++) {
			if (read(fd, &utmp, size) != size)
				break;
			/* edwardc.010813 watch point PR:1101 start */
			val = compute_user_value(&utmp);
			if (utmp.userid[0] != '\0' && val < 0) {
				sprintf(genbuf, "#%d %-12s %15.15s %d %d %d",
					i + 1, utmp.userid, ctime(&(utmp.lastlogin)) + 4,
					utmp.numlogins, utmp.numposts, val);
				log_usies("KILL ", genbuf);
				if (!bad_user_id(utmp.userid)) {
					sethomefile(genbuf, utmp.userid, "mentor");
					if (dashf(genbuf)) {
						if ((fn = fopen(genbuf, "r")) != NULL) {
							if (fgets(buf, 20, fn) != NULL) {
								if ((ptr = strchr(buf, '\n')) != NULL)
									*ptr = '\0';
								sethomefile(buf2, buf, "downline");
								del_from_file(buf2, utmp.userid);
							}
						}
					}
					sprintf(genbuf, "mail/%c/%s",
						toupper(utmp.userid[0]), utmp.userid);
					f_rm(genbuf);
					sprintf(genbuf, "home/%c/%s",
						toupper(utmp.userid[0]), utmp.userid);
					f_rm(genbuf);
				}
				lseek(fd, (off_t) (-size), SEEK_CUR);
				write(fd, &zerorec, sizeof(utmp));
			}
			/* end of watch point PR:1101 */
		}
		close(fd);
		touchnew();
	}
	if ((fd = open(PASSFILE, O_RDWR | O_CREAT, 0600)) == -1)
		return -1;
	f_exlock(fd);

	i = searchnewuser();
	sprintf(genbuf, "uid %d from %s", i, fromhost);
	log_usies("APPLY", genbuf);

	if (i <= 0 || i > MAXUSERS) {
		f_unlock(fd);
		close(fd);
		if (dashf(F_USER_FULL)) {
			ansimore(F_USER_FULL, NA);
			oflush();
		} else {
			printf("抱歉, 使用者帳號已經滿了, 無法註冊新的帳號.\n\r");
		}
		val = (st.st_mtime - system_time + 3660) / 60;
		printf("請等待 %d 分鐘後再試一次, 祝你好運.\n\r", val);
		sleep(2);
		exit(1);
	}
	memset(&utmp, 0, sizeof(utmp));
	strcpy(utmp.userid, "new");
	utmp.lastlogin = time(NULL);
	if (lseek(fd, (off_t) (sizeof(utmp) * (i - 1)), SEEK_SET) == -1) {
		f_unlock(fd);
		close(fd);
		return -1;
	}
	write(fd, &utmp, sizeof(utmp));
	setuserid(i, utmp.userid);
	f_unlock(fd);
	close(fd);
	return i;
}

int
id_with_num(userid)
char    userid[IDLEN + 1];
{
	char   *s;
	for (s = userid; *s != '\0'; s++) {
		if (*s < 1 || !isalpha(*s))
			return 1;
	}
	return 0;
}

void
new_register()
{
	struct userec newuser;
	char    passbuf[STRLEN];
	int     allocid, try;

/*
	edwardc.020903 weired code, seems to be detect collision, but not sure ..
	disable for now ya.
	
	if (1) {
		time_t  now;
		now = time(0);
		sprintf(genbuf, "etc/no_register_%3.3s", ctime(&now));
		if (dashf(genbuf)) {
			ansimore(genbuf, NA);
			pressreturn();
			exit(1);
		}
	}
*/
	ansimore(F_REGISTER, NA);
	try = 0;
	while (1) {
		if (++try >= 9) {
			prints("\n掰掰，按太多下  <Enter> 了...\n");
			refresh();
			longjmp(byebye, -1);
		}
		getdata(0, 0, "請輸入帳號名稱 (Enter User ID, \"0\" to abort): ", passbuf, IDLEN + 1, DOECHO, YEA);
		if (passbuf[0] == '0') {
			longjmp(byebye, -1);
		}
		if (id_with_num(passbuf)) {
			prints("帳號必須全為英文字母!\n");
		} else if (strlen(passbuf) < 2) {
			prints("帳號至少需有兩個英文字母!\n");
		} else if ((*passbuf == '\0') || bad_user_id(passbuf)) {
			prints("抱歉, 您不能使用這個字作為帳號。 請想過另外一個。\n");
		} else if (dosearchuser(passbuf)) {
			prints("此帳號已經有人使用\n");
		} else
			break;
	}

	memset(&newuser, 0, sizeof(newuser));
	allocid = getnewuserid();
	if (allocid > MAXUSERS || allocid <= 0) {
		printf("No space for new users on the system!\n\r");
		exit(1);
	}
	strcpy(newuser.userid, passbuf);
	strcpy(passbuf, "");


	while (1) {

		if (!strcmp(newuser.userid, "guest")) {
			/* edwardc.010816 隨便給他一個, 反正又沒差 */
			strncpy(newuser.passwd, genpasswd("blah123"), ENCPASSLEN);
			break;
		}
		getdata(0, 0, "請設定您的密碼 (Setup Password): ", passbuf, PASSLEN, NOECHO, YEA);
		if (strlen(passbuf) < 4 || !strcmp(passbuf, newuser.userid)) {
			prints("密碼太短或與使用者代號相同, 請重新輸入\n");
			continue;
		}
		strncpy(newuser.passwd, passbuf, PASSLEN);
		getdata(0, 0, "請再輸入一次你的密碼 (Reconfirm Password): ", passbuf, PASSLEN, NOECHO, YEA);
		if (strncmp(passbuf, newuser.passwd, PASSLEN) != 0) {
			prints("密碼輸入錯誤, 請重新輸入密碼.\n");
			continue;
		}
		passbuf[8] = '\0';
		strncpy(newuser.passwd, genpasswd(passbuf), ENCPASSLEN);
		break;
	}

	strcpy(newuser.termtype, "vt100");
	newuser.userdefine = -1;
	if (!strcmp(newuser.userid, "guest")) {
		newuser.userlevel = 0;
		newuser.flags[0] = CURSOR_FLAG;
		newuser.userdefine &= ~(DEF_FRIENDCALL | DEF_ALLMSG | DEF_FRIENDMSG);
	} else {
		newuser.userlevel = PERM_BASIC;
		newuser.flags[0] = CURSOR_FLAG | PAGER_FLAG;
	}
	newuser.userdefine &= ~(DEF_NOLOGINSEND);
	newuser.flags[1] = 0;
	newuser.firstlogin = newuser.lastlogin = time(NULL);

	if (substitute_record(PASSFILE, &newuser, sizeof(newuser), allocid) == -1) {
		fprintf(stderr, "too much, good bye!\n");
		exit(1);
	}
	setuserid(allocid, newuser.userid);
	if (!dosearchuser(newuser.userid)) {
		fprintf(stderr, "User failed to create\n");
		exit(1);
	}
	report("new account");
}

char   *
trim(s)
char   *s;
{
	char   *buf;
	char   *l, *r;
	buf = (char *)malloc(256);

	buf[0] = '\0';
	r = s + strlen(s) - 1;

	for (l = s; strchr(" \t\r\n", *l) && *l; l++);

	/* if all space, *l is null here, we just return null */
	if (*l != '\0') {
		for (; strchr(" \t\r\n", *r) && r >= l; r--);
		strncpy(buf, l, r - l + 1);
	}
	return buf;
}

int
invalid_email(addr)
char   *addr;
{
	FILE   *fp;
	char    temp[STRLEN], *ptr;

	if (strstr(addr, "bbs") != NULL || strstr("bbs", addr) != NULL)
		return 1;

	if ((fp = fopen(F_BAD_EMAIL, "r")) != NULL) {
		while (fgets(temp, STRLEN, fp) != NULL) {
			ptr = strtok(temp, "\n\t\r");
			if (ptr != NULL && *ptr != '#' && strcasecmp_match(addr, ptr) == 0) {
				fclose(fp);
				return 1;
			}
		}
		fclose(fp);
	}
	return 0;
}

int
invalid_realmail(userid, email)
char   *userid, *email;
{
	FILE   *fn;
	char   *emailfile, fname[STRLEN];
	struct stat st;
	time_t  now;
	if ((emailfile = sysconf_str("EMAILFILE")) == NULL)
		return 0;

	if (strchr(email, '@') && valid_ident(email) && HAS_PERM(PERM_LOGINOK))
		return 0;

	now = time(0);
	sethomefile(fname, userid, HF_REGISTER);
	if (stat(fname, &st) == 0) {
		if (now - st.st_mtime >= REG_EXPIRED * 86400) {
			sethomefile(fname, userid, HF_REGISTER_OLD);
			if (stat(fname, &st) == -1 || now - st.st_mtime >= REG_EXPIRED * 86400)
				return 1;
		}
	}
	sethomefile(fname, userid, HF_REGISTER);
	if ((fn = fopen(fname, "r")) != NULL) {
		fgets(genbuf, STRLEN, fn);
		fclose(fn);
		strtok(genbuf, "\n");
		if (valid_ident(genbuf) &&
			((strchr(genbuf, '@') != NULL) || strstr(genbuf, "usernum"))) {
			if (strchr(genbuf, '@') != NULL) {
				genbuf[STRLEN - 17] = '\0';
				strncpy(email, genbuf, STRLEN - 17);
			}
			move(21, 0);
			prints("恭賀您!! 您已順利完成本站的使用者註冊手續,\n");
			prints("從現在起您將擁有一般使用者的權利與義務...\n");
			pressanykey();
			return 0;
		}
	}
	return 1;
}
#ifdef CODE_VALID
char   *
genrandpwd(int seed)
{
	char    panel[] = "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char   *result;
	int     i, rnd;
	result = (char *)malloc(RNDPASSLEN + 1);

	srand((unsigned)(time(0) * seed));
	bzero(result, RNDPASSLEN + 1);

	for (i = 0; i < RNDPASSLEN; i++) {
		rnd = rand() % sizeof(panel);
		if (panel[rnd] == '\0') {
			i--;
			continue;
		}
		result[i] = panel[rnd];
	}

	sethomefile(genbuf, currentuser.userid, HF_REGPASS);
	unlink(genbuf);
	file_append(genbuf, result);

	return ((char *)result);
}
#endif

void
send_regmail(trec)
struct userec *trec;
{
	time_t  code;
	FILE   *fin, *fout, *dp;
#ifdef CODE_VALID
	char    buf[RNDPASSLEN + 1];
#endif

	sethomefile(genbuf, trec->userid, HF_MAILCHECK);
	if ((dp = fopen(genbuf, "w")) == NULL) {
		fclose(dp);
		return;
	}
	code = time(0);
	fprintf(dp, "%9.9d:%d\n", code, getpid());
	fclose(dp);

	sprintf(genbuf, "%s -f %s.bbs@%s %s ", SENDMAIL,
		trec->userid, BBSHOST, trec->email);
	fout = popen(genbuf, "w");
	fin = fopen(sysconf_str("EMAILFILE"), "r");
	if ((fin != NULL) && (fout != NULL)) {
		fprintf(fout, "Reply-To: SYSOP.bbs@%s\n", BBSHOST);
		fprintf(fout, "From: SYSOP.bbs@%s\n", BBSHOST);
		fprintf(fout, "To: %s\n", trec->email);
		fprintf(fout, "Subject: @%s@[-%9.9d:%d-]%s mail check.\n", trec->userid, code, getpid(), BBSID);
		fprintf(fout, "X-Purpose: %s registration mail.\n", BBSNAME);
		fprintf(fout, "X-Priority: 1 (Highest)\n");
		fprintf(fout, "X-MSMail-Priority: High\n");
		fprintf(fout, "\n");
		fprintf(fout, "[中文]\n");
		fprintf(fout, "BBS 位址         : %s (%s)\n", BBSHOST, BBSIP);
		fprintf(fout, "您註冊的 BBS ID  : %s\n", trec->userid);
		fprintf(fout, "登錄日期         : %s", ctime(&trec->firstlogin));
		fprintf(fout, "登入來源         : %s\n", fromhost);
		fprintf(fout, "您的真實姓名/暱稱: %s (%s)\n", trec->realname, trec->username);
#ifdef CODE_VALID
		sprintf(buf, "%s", (char *)genrandpwd((int)getpid()));
		fprintf(fout, "認證暗碼         : %s (請注意大小寫)\n", buf);
#endif
		fprintf(fout, "認證信發出日期   : %s\n", ctime(&code));

		fprintf(fout, "[English]\n");
		fprintf(fout, "BBS LOCATION     : %s (%s)\n", BBSHOST, BBSIP);
		fprintf(fout, "YOUR BBS USER ID : %s\n", trec->userid);
		fprintf(fout, "CREATION DATE    : %s", ctime(&trec->firstlogin));
		fprintf(fout, "LOGIN HOST       : %s\n", fromhost);
		fprintf(fout, "YOUR NICK NAME   : %s\n", trec->username);
		fprintf(fout, "YOUR NAME        : %s\n", trec->realname);
#ifdef CODE_VALID
		fprintf(fout, "VALID CODE       : %s (case sensitive)\n", buf);
#endif
		fprintf(fout, "THIS MAIL SENT ON: %s\n", ctime(&code));

		while (fgets(genbuf, 255, fin) != NULL) {
			if (genbuf[0] == '.' && genbuf[1] == '\n')
				fputs(". \n", fout);
			else
				fputs(genbuf, fout);
		}
		fprintf(fout, ".\n");
		fclose(fin);
		fclose(fout);
	}
}

void
check_gender()
{
	char    ans[5];
	time_t  now;
	struct tm *tmnow;
	clear();

	now = time(0);
	tmnow = localtime(&now);

	ans[0] = '\0';
	while (ans[0] < '1' || ans[0] > '2') {
		getdata(2, 0, "請輸入你的性別: [1]男的 [2]女的 (1 or 2): ",
			ans, 2, DOECHO, YEA);
	}
	switch (ans[0]) {
	case '1':
		currentuser.gender = 'M';
		break;
	case '2':
		currentuser.gender = 'F';
		break;
	}
	move(4, 0);
	prints("請輸入您的出生日期");
	while (currentuser.birthyear < tmnow->tm_year - 98 || currentuser.birthyear > tmnow->tm_year - 3) {
		ans[0] = '\0';
		getdata(5, 0, "四位數公元年: ", ans, 5, DOECHO, YEA);
		if (atoi(ans) < 1900)
			continue;
		currentuser.birthyear = atoi(ans) - 1900;
	}
	while (currentuser.birthmonth < 1 || currentuser.birthmonth > 12) {
		ans[0] = '\0';
		getdata(6, 0, "出生月: (1-12) ", ans, 3, DOECHO, YEA);
		currentuser.birthmonth = atoi(ans);
	}
	while (currentuser.birthday < 1 || currentuser.birthday > 31) {
		ans[0] = '\0';
		getdata(7, 0, "出生日: (1-31) ", ans, 3, DOECHO, YEA);
		currentuser.birthday = atoi(ans);
	}

	substitute_record(PASSFILE, &currentuser, sizeof(currentuser), usernum);
}
void
check_register_info()
{
	struct userec *urec = &currentuser;
	char   *newregfile;
	FILE   *fout;
	int     perm;
	char    ans[4], buf[192], buf2[STRLEN];
	extern int showansi;
	time_t  now;
	struct tm *tmnow;
	char    uident[STRLEN];
#ifdef CODE_VALID
	int     i;
#endif

	clear();
	if (!(urec->userlevel & PERM_BASIC)) {
		urec->userlevel = 0;
		return;
	}
	perm = PERM_DEFAULT & sysconf_eval("AUTOSET_PERM");

	while ((strlen(urec->username) < 2)
		|| (strstr(urec->username, "  "))
		|| (strstr(urec->username, "　"))) {
		getdata(2, 0, "請輸入您的暱稱 (Enter nickname): ", urec->username, NAMELEN, DOECHO, YEA);
		strcpy(uinfo.username, urec->username);
		update_utmp();
	}
	while ((strlen(urec->realname) < 4)
		|| (strstr(urec->realname, "  "))
		|| (strstr(urec->realname, "　"))) {
		move(3, 0);
		prints("請輸入您的真實姓名 (Enter realname):\n");
		getdata(4, 0, "> ", urec->realname, NAMELEN, DOECHO, YEA);
	}
	while ((strlen(urec->address) < 10) || (strstr(urec->address, "   "))) {
		move(5, 0);
		prints("請輸入您的通訊地址 (Enter home address)：\n");
		getdata(6, 0, "> ", urec->address, STRLEN - 10, DOECHO, YEA);
	}
	if (strchr(urec->email, '@') == NULL) {
		move(8, 0);
		prints("電子信箱格式為: [1;37muserid@your.domain.name[m\n");
		prints("請輸入電子信箱 (不能提供者按 <Enter>)");
		getdata(10, 0, "> ", urec->email, NAMELEN, DOECHO, YEA);
		if (strchr(urec->email, '@') == NULL) {
			sprintf(genbuf, "%s.bbs@%s", urec->userid, BBSHOST);
			strncpy(urec->email, genbuf, STRLEN);
		}
	}
	now = time(0);
	tmnow = localtime(&now);

	if (!HAS_PERM(PERM_LOGINOK)) {
		if (!invalid_realmail(urec->userid, urec->reginfo)) {
			set_safe_record();
			urec->lastjustify = time(0);
			urec->userlevel |= PERM_DEFAULT;
			if (HAS_PERM(PERM_DENYPOST) && !HAS_PERM(PERM_SYSOP))
				urec->userlevel &= ~PERM_POST;
			substitute_record(PASSFILE, urec, sizeof(struct userec), usernum);
#ifdef CODE_VALID
		} else {
			/* edwardc.990426 暗碼認證 */
			sethomefile(buf, currentuser.userid, HF_REGPASS);
			if (dashf(buf)) {

				move(13, 0);
				prints("您尚未通過身份確認... \n");
				prints("您現在必須輸入註冊確認信裡, \"認證暗碼\"來做為身份確認\n");
				prints("一共是 %d 個字元, 大小寫是有差別的, 請注意. \n", RNDPASSLEN);
				prints("若您想取消可以連按三下 [Enter] 鍵, 系統將會重新發出一封確認信.\n");
				prints("[1;33m請注意, 請您輸入最新一封認證信中所包含的亂數密碼！[m\n");

				if ((fout = fopen(buf, "r")) != NULL) {
					fscanf(fout, "%s", buf2);
					fclose(fout);
				}
				for (i = 0; i < 3; i++) {
					move(18, 0);
					prints("你還有 %d 次機會\n", 3 - i);
					getdata(19, 0, "請輸入認證暗碼: ", genbuf, (RNDPASSLEN + 1), DOECHO, YEA);

					if (strcmp(genbuf, "") != 0) {
						if (strcmp(genbuf, buf2) != 0)
							continue;
						else
							break;
					}
				}

				if (i == 3) {
					prints("暗碼認證失敗! 系統將重新發出認證信, 若有疑問請至 SYSOP 板發表\n");
					send_regmail(&currentuser);
					pressanykey();
				} else {
					set_safe_record();
					urec->userlevel |= PERM_DEFAULT;
					if (HAS_PERM(PERM_DENYPOST) && !HAS_PERM(PERM_SYSOP))
						urec->userlevel &= ~PERM_POST;
					/*
					 * edwardc.990502
					 * 修正會一直要求重新註冊的問題
					 */
					urec->lastjustify = time(0);
					/*
					 * edwardc.991019 把 e-mail 寫入 "真
					 * -mail"
					 */
					strncpy(urec->reginfo, urec->email, 62);

					substitute_record(PASSFILE, urec, sizeof(struct userec), usernum);
					prints("恭賀您!! 您已順利完成本站的使用者註冊手續,\n");
					prints("從現在起您將擁有一般使用者的權利與義務...\n");
					unlink(buf);
					mail_file(F_SUCC_EMAIL, currentuser.userid, "歡迎加入本站行列");
					pressanykey();
				}

			} else
#else
		} else {
#endif
			if ((sysconf_str("EMAILFILE") != NULL) &&
				(!strstr(urec->email, buf)) &&
				(!invalidaddr(urec->email)) &&
				(!invalid_email(urec->email))) {

				sethomefile(buf, currentuser.userid, HF_REGPASS);
				if (dashf(buf)) {
					if (sysconf_str("NEWREGFILE") != NULL) {
						showansi = 1;
						prints("您以通過身份確認，但尚未經過新手三天限制\n");
						prints("請耐心等候 :)\n");
						pressanykey();
					}
					return;
				}
				move(13, 0);

				prints("您的電子信箱 尚須通過回信驗證...  \n");
				prints("    本站將馬上寄一封驗證信給您,\n");
				prints("    您只要從 %s 回信, 就可以成為本站合格公民.\n\n", urec->email);
				prints("    成為本站合格公民, 就能享有更多的權益喔!\n");
				move(20, 0);
				if (askyn("您要我們現在就寄這一封信嗎", YEA, NA) == YEA) {
					send_regmail(urec);
					getdata(21, 0, "確認信已寄出, 等您回信哦!! 請按 <Enter> : ", ans, 2, DOECHO, YEA);
				}
			} else {
				showansi = 1;
				if (sysconf_str("EMAILFILE") != NULL) {
					prints("\n您所填寫的電子郵件地址 【[1;33m%s[m】\n", urec->email);
					prints("恕不受本站承認，系統不會投遞註冊信，請到[1;32mInfoEdit->Info[m中修改...\n");
					pressanykey();
				}
			}
		}
	}
	if (urec->lastlogin - urec->firstlogin < 3 * 86400) {
		if (urec->numlogins == 1) {

			clear();
			prints("如果您是由本站站友介紹而來, 請輸入對方之帳號, 否則就按 <Enter> 跳過");
			do {
				move(1, 0);
				usercomplete("介紹人: ", uident);
				if ((getuser(uident) <= 0) || !strcmp(uident, currentuser.userid))
					uident[0] = '\0';
				if (uident[0] == '\0') {
					sprintf(buf, "沒有介紹人, 確定嗎");
				} else {
					sprintf(buf, "介紹人為 %s, 確定嗎", uident);
				}
			} while (askyn(buf, YEA, NA) == NA);
			if (uident[0] != '\0') {
				setuserfile(buf, HF_MENTOR);
				file_append(buf, uident);
				sethomefile(buf, uident, HF_DOWNLINE);
				file_append(buf, currentuser.userid);
			}
			mail_file(F_MENTOR_NOT, uident, "朋友註冊帳號通知");
			move(2, 0);
			clrtobot();
			ans[0] = '\0';
			while (ans[0] < '1' || ans[0] > '2') {
				getdata(2, 0, "請輸入你的性別: [1]男的 [2]女的 (1 or 2): ",
					ans, 2, DOECHO, YEA);
			}
			switch (ans[0]) {
			case '1':
				urec->gender = 'M';
				break;
			case '2':
				urec->gender = 'F';
				break;
			}
			move(4, 0);
			prints("請輸入您的出生日期");
			while (urec->birthyear < tmnow->tm_year - 98 || urec->birthyear > tmnow->tm_year - 3) {
				ans[0] = '\0';
				getdata(5, 0, "四位數公元年: ", ans, 5, DOECHO, YEA);
				if (atoi(ans) < 1900)
					continue;
				urec->birthyear = atoi(ans) - 1900;
			}
			while (urec->birthmonth < 1 || urec->birthmonth > 12) {
				ans[0] = '\0';
				getdata(6, 0, "出生月: (1-12) ", ans, 3, DOECHO, YEA);
				urec->birthmonth = atoi(ans);
			}
			while (urec->birthday < 1 || urec->birthday > 31) {
				ans[0] = '\0';
				getdata(7, 0, "出生日: (1-31) ", ans, 3, DOECHO, YEA);
				urec->birthday = atoi(ans);
			}
			sprintf(buf, "tmp/newcomer.%s", currentuser.userid);
			if ((fout = fopen(buf, "w")) != NULL) {
				fprintf(fout, "大家好,\n\n");
				fprintf(fout, "我是 %s (%s), 來自 %s\n"
					,currentuser.userid, urec->username, fromhost);
				if (uident[0] != '\0')
					fprintf(fout, "很感謝 %s 把我帶來這裡。\n\n", uident);
				fprintf(fout, "今天%s初來此站報到, 請大家多多指教。\n",
					(urec->gender == 'M') ? "小弟" : "小女子");
				move(9, 0);
				prints("請作個簡短的個人簡介, 向本站其他使用者打個招呼\n");
				prints("(最多三行, 寫完可直接按 <Enter> 跳離)....");
				getdata(11, 0, ":", buf2, 75, DOECHO, YEA);
				if (buf2[0] != '\0') {
					fprintf(fout, "\n\n自我介紹:\n\n");
					fprintf(fout, "%s\n", buf2);
					getdata(12, 0, ":", buf2, 75, DOECHO, YEA);
					if (buf2[0] != '\0') {
						fprintf(fout, "%s\n", buf2);
						getdata(13, 0, ":", buf2, 75, DOECHO, YEA);
						if (buf2[0] != '\0') {
							fprintf(fout, "%s\n", buf2);
						}
					}
				}
				fclose(fout);
				sprintf(buf2, "新手上路: %s", urec->username);
				postfile(buf, "newcomers", buf2, 2);
				unlink(buf);
			}
			pressanykey();
		}
		newregfile = sysconf_str("NEWREGFILE");
		if (!HAS_PERM(PERM_SYSOP) && newregfile != NULL) {
			set_safe_record();
			urec->userlevel &= ~(perm);
			substitute_record(PASSFILE, urec, sizeof(struct userec), usernum);
			ansimore(newregfile, YEA);
		}
	}
	set_safe_record();
	if (HAS_PERM(PERM_DENYPOST) && !HAS_PERM(PERM_SYSOP)) {
		currentuser.userlevel &= ~PERM_CHAT;
		currentuser.userlevel &= ~PERM_PAGE;
		currentuser.userlevel &= ~PERM_POST;
		currentuser.userlevel &= ~PERM_LOGINOK;
		uinfo.pager &= ~ALL_PAGER;
		uinfo.pager &= ~FRIEND_PAGER;
		substitute_record(PASSFILE, urec, sizeof(struct userec), usernum);
	}
}
