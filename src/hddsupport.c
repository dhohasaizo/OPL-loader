#include "sys/fcntl.h"
#include "include/usbld.h"
#include "include/lang.h"
#include "include/hddsupport.h"
#include "include/textures.h"
#include "include/themes.h"
#include "include/ioman.h"
#include "include/system.h"

extern void *hdd_cdvdman_irx;
extern int size_hdd_cdvdman_irx;

extern void *hdd_pcmcia_cdvdman_irx;
extern int size_hdd_pcmcia_cdvdman_irx;

extern void *ps2dev9_irx;
extern int size_ps2dev9_irx;

extern void *ps2atad_irx;
extern int size_ps2atad_irx;

extern void *ps2hdd_irx;
extern int size_ps2hdd_irx;

extern void *ps2fs_irx;
extern int size_ps2fs_irx;

static int hddForceUpdate = 1;
static char *hddPrefix = NULL;
static int hddGameCount = 0;
static hdl_games_list_t* hddGames;

// forward declaration
static item_list_t hddGameList;

static void hddLoadModules(void) {
	int ret;
	static char hddarg[] = "-o" "\0" "4" "\0" "-n" "\0" "20";

	LOG("hddLoadModules()\n");

	gHddStartup = 4;

	ret = sysLoadModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL);
	if (ret < 0) {
		gHddStartup = -1;
		return;
	}

	gHddStartup = 3;

	ret = sysLoadModuleBuffer(&ps2atad_irx, size_ps2atad_irx, 0, NULL);
	if (ret < 0) {
		gHddStartup = -1;
		return;
	}

	gHddStartup = 2;

	ret = sysLoadModuleBuffer(&ps2hdd_irx, size_ps2hdd_irx, sizeof(hddarg), hddarg);
	if (ret < 0) {
		gHddStartup = -1;
		return;
	}

	gHddStartup = 1;

	ret = sysLoadModuleBuffer(&ps2fs_irx, size_ps2fs_irx, 0, NULL);
	if (ret < 0) {
		gHddStartup = -1;
		return;
	}

	LOG("hddLoadModules: modules loaded\n");

	ret = fileXioMount(hddPrefix, "hdd0:+OPL", FIO_MT_RDONLY);
	if (ret < 0) {
		fileXioUmount(hddPrefix);
		fileXioMount(hddPrefix, "hdd0:+OPL", FIO_MT_RDONLY);
	}

	gHddStartup = 0;

	// update Themes
	char path[32];
	sprintf(path, "%sTHM/", hddPrefix);
	thmAddElements(path, "/");
}

void hddInit(void) {
	LOG("hddInit()\n");
	hddPrefix = "pfs0:";
	hddForceUpdate = 1;
	//hddGames = NULL;

	ioPutRequest(IO_CUSTOM_SIMPLEACTION, &hddLoadModules);

	hddGameList.enabled = 1;
}

item_list_t* hddGetObject(int initOnly) {
	if (initOnly && !hddGameList.enabled)
		return NULL;
	return &hddGameList;
}

static int hddNeedsUpdate(void) {
	if (hddForceUpdate) {
		hddForceUpdate = 0;
		return 1;
	}
	
	return 0;
}

static int hddUpdateGameList(void) {
	int ret = hddGetHDLGamelist(&hddGames);
	if (ret != 0)
		hddGameCount = 0;
	else
		hddGameCount = hddGames->count;
	return hddGameCount;
}

static int hddGetGameCount(void) {
	return hddGameCount;
}

static void* hddGetGame(int id) {
	return (void*) &hddGames->games[id];
}

static char* hddGetGameName(int id) {
	return hddGames->games[id].name;
}

static char* hddGetGameStartup(int id) {
	return hddGames->games[id].startup;
}

#ifndef __CHILDPROOF
static void hddDeleteGame(int id) {
	hddDeleteHDLGame(&hddGames->games[id]);
	hddForceUpdate = 1;
}

static void hddRenameGame(int id, char* newName) {
	hdl_game_info_t* game = &hddGames->games[id];
	strcpy(game->name, newName);
	hddSetHDLGameInfo(&hddGames->games[id]);
	hddForceUpdate = 1;
}
#endif

static int hddGetGameCompatibility(int id, int *dmaMode) {
	return configGetCompatibility(hddGames->games[id].startup, hddGameList.mode, dmaMode);
}

static void hddSetGameCompatibility(int id, int compatMode, int dmaMode) {
	configSetCompatibility(hddGames->games[id].startup, hddGameList.mode, compatMode, dmaMode);
}

static int hddLaunchGame(int id) {
	int i, size_irx = 0;
	void** irx = NULL;
	char filename[32];

	hdl_game_info_t* game = &hddGames->games[id];

	if (gRememberLastPlayed) {
		configSetStr(configGetByType(CONFIG_OPL), "last_played", game->startup);
		saveConfig(CONFIG_OPL, 0);
	}

	char gid[5];
	configGetDiscIDBinary(hddGames->games[id].startup, gid);

	int dmaType = 0, dmaMode = 0, compatMode = 0;
	compatMode = configGetCompatibility(hddGames->games[id].startup, hddGameList.mode, &dmaMode);
	if(dmaMode < 3)
		dmaType = 0x20;
	else {
		dmaType = 0x40;
		dmaMode -= 3;
	}
	hddSetTransferMode(dmaType, dmaMode);

	if (sysPcmciaCheck()) {
		size_irx = size_hdd_pcmcia_cdvdman_irx;
		irx = &hdd_pcmcia_cdvdman_irx;
	}
	else {
		size_irx = size_hdd_cdvdman_irx;
		irx = &hdd_cdvdman_irx;
	}

	for (i=0;i<size_irx;i++){
		if(!strcmp((const char*)((u32)irx+i),"######    GAMESETTINGS    ######")){
			break;
		}
	}

	if (compatMode & COMPAT_MODE_2) {
		u32 alt_read_mode = 1;
		memcpy((void*)((u32)irx+i+35),&alt_read_mode,1);
	}
	if (compatMode & COMPAT_MODE_5) {
		u32 no_dvddl = 1;
		memcpy((void*)((u32)irx+i+36),&no_dvddl,4);
	}
	if (compatMode & COMPAT_MODE_4) {
		u32 no_pss = 1;
		memcpy((void*)((u32)irx+i+40),&no_pss,4);
	}

	// game id
	memcpy((void*)((u32)irx+i+84), &gid, 5);

	// patch 48bit flag
	u8 flag_48bit = hddIs48bit() & 0xff;
	memcpy((void*)((u32)irx+i+34), &flag_48bit, 1);

	// patch start_sector
	memcpy((void*)((u32)irx+i+44), &game->start_sector, 4);

	// patches cdvdfsv
	void *cdvdfsv_irx;
	int size_cdvdfsv_irx;

	sysGetCDVDFSV(&cdvdfsv_irx, &size_cdvdfsv_irx);
	u32 *p = (u32 *)cdvdfsv_irx;
	for (i = 0; i < (size_cdvdfsv_irx >> 2); i++) {
		if (*p == 0xC0DEC0DE) {
			if (compatMode & COMPAT_MODE_7)
				*p = 1;
			else
				*p = 0;
			break;
		}
		p++;
	}

	sprintf(filename,"%s",game->startup);
	shutdown(NO_EXCEPTION); // CAREFUL: shutdown will call hddCleanUp, so hddGames/game will be freed
	FlushCache(0);

	sysLaunchLoaderElf(filename, "HDD_MODE", size_irx, irx, compatMode, compatMode & COMPAT_MODE_1);

	return 1;
}

static int hddGetArt(char* name, GSTEXTURE* resultTex, const char* type, short psm) {
	char path[32];
	sprintf(path, "%sART/%s_%s", hddPrefix, name, type);
	return texDiscoverLoad(resultTex, path, -1, psm);
}

static void hddCleanUp(int exception) {
	if (hddGameList.enabled) {
		LOG("hddCleanUp()\n");

		hddFreeHDLGamelist(hddGames);

		if ((exception & UNMOUNT_EXCEPTION) == 0)
			fileXioUmount(hddPrefix);
	}
}

static item_list_t hddGameList = {
		HDD_MODE, HDL_GAME_NAME_MAX + 1, 0, 0, 0, "HDD Games", _STR_HDD_GAMES, &hddInit, &hddNeedsUpdate, &hddUpdateGameList,
#ifdef __CHILDPROOF
		&hddGetGameCount, &hddGetGame, &hddGetGameName, &hddGetGameStartup, NULL, NULL, &hddGetGameCompatibility,
#else
		&hddGetGameCount, &hddGetGame, &hddGetGameName, &hddGetGameStartup, &hddDeleteGame, &hddRenameGame, &hddGetGameCompatibility,
#endif
		&hddSetGameCompatibility, &hddLaunchGame, &hddGetArt, &hddCleanUp, HDD_ICON
};
