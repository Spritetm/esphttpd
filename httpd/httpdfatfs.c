/*
Connector to let httpd use the fat filesystem on eg an SD-card to serve the files in it.
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

#include <esp8266.h>
#include "httpdfatfs.h"
#include "ff.h"

int ICACHE_FLASH_ATTR cgiFatFsDirHook(HttpdConnData *connData) {
	DIR *fp=connData->cgiData;
	char buff[512];
	static TCHAR lfn[256];
	int needSlash;
	FRESULT fr;
	FILINFO fi;
	TCHAR *nm;

	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		if (fp!=NULL) {
			f_closedir(fp);
			os_free(fp);
		}
		return HTTPD_CGI_DONE;
	}

	if (connData->url[strlen(connData->url)-1]=='/') needSlash=0; else needSlash=1;
	if (fp==NULL) {
		//First call to this cgi. Open the file so we can read it.
		fp=(DIR *)os_malloc(sizeof(DIR));
		fr=f_opendir(fp, (const TCHAR *)&connData->url[1]);
		if (fr!=FR_OK) {
			os_free(fp);
			return HTTPD_CGI_NOTFOUND;
		}
		connData->cgiData=fp;
		httpdStartResponse(connData, 200);
		httpdHeader(connData, "Content-Type", httpdGetMimetype(connData->url));
		httpdHeader(connData, "Cache-Control", "max-age=3600, must-revalidate");
		httpdEndHeaders(connData);
		os_sprintf(buff, "<html><head><title>Index of %s</title></head><body><h2>Index of %s</h2><ul>\n", connData->url, connData->url);
		httpdSend(connData, buff, -1);
		return HTTPD_CGI_MORE;
	}

	fi.lfname=lfn;
	fi.lfsize=sizeof(lfn);
	fr=f_readdir(fp, &fi);
	if (fr==FR_OK && fi.fname[0]!=0) {
		nm=fi.fname;
		if (lfn[0]!=0) nm=lfn;
		os_sprintf(buff, "<li><a href=\"%s%s%s\">%s</a></li>\n", connData->url, needSlash?"/":"", (char*)nm, (char*)nm);
		httpdSend(connData, buff, -1);
		//Ok, till next time.
		return HTTPD_CGI_MORE;
	} else {
		//We're done.
		f_closedir(fp);
		os_free(fp);
		os_sprintf(buff, "</ul></body></html>");
		httpdSend(connData, buff, -1);
		return HTTPD_CGI_DONE;
	}
	
}


//This is a catch-all cgi function. It takes the url passed to it, looks up the corresponding
//path in the filesystem and if it exists, passes the file through. This simulates what a normal
//webserver would do with static files.
int ICACHE_FLASH_ATTR cgiFatFsHook(HttpdConnData *connData) {
	FIL *fp=connData->cgiData;
	unsigned int len;
	char buff[1024*2];
	FRESULT fr;
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		if (fp!=NULL) {
			f_close(fp);
			os_free(fp);
		}
		return HTTPD_CGI_DONE;
	}

	if (fp==NULL) {
		//First call to this cgi. Open the file so we can read it.
		fp=(FIL *)os_malloc(sizeof(FIL));
		fr=f_open(fp, (const TCHAR *)&connData->url[1], FA_READ);
		if (fr!=FR_OK) {
			os_free(fp);
			return HTTPD_CGI_NOTFOUND;
		}
		connData->cgiData=fp;
		httpdStartResponse(connData, 200);
		httpdHeader(connData, "Content-Type", httpdGetMimetype(connData->url));
//		httpdHeader(connData, "Cache-Control", "max-age=3600, must-revalidate");
		httpdEndHeaders(connData);
		return HTTPD_CGI_MORE;
	}

	fr=f_read(fp, buff, sizeof(buff), &len);
	os_printf("Read %d bytes for %s\n", len, connData->url);
	if (len>0) espconn_sent(connData->conn, (uint8 *)buff, len);
	if (len!=sizeof(buff)) {
		//We're done.
		f_close(fp);
		os_free(fp);
		return HTTPD_CGI_DONE;
	} else {
		//Ok, till next time.
		return HTTPD_CGI_MORE;
	}
}

