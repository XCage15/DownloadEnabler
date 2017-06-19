/*
	Download Enabler
	Copyright (C) 2017, TheFloW

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <psp2/photoexport.h>
#include <psp2/sysmodule.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/modulemgr.h>

#include <taihen.h>

static tai_hook_ref_t GetFileTypeRef;
static tai_hook_ref_t sceSysmoduleLoadModuleInternalRef;
static tai_hook_ref_t sceSysmoduleUnloadModuleInternalRef;
static tai_hook_ref_t scePhotoExportFromFile2Ref;

static SceUID hooks[5];

static int GetFileTypePatched(int unk, int *type, char **filename, char **mime_type) {
	int res = TAI_CONTINUE(int, GetFileTypeRef, unk, type, filename, mime_type);

	if (res < 0) {
		*type = 1; // Type photo
		return 0;
	}

	return res;
}

static int scePhotoExportFromFile2Patched(const char *path, const PhotoExportParam *param, void *workingMemory, void *cancelCb, void *user, char *outPath, SceSize outPathSize) {
	int res = TAI_CONTINUE(int, scePhotoExportFromFile2Ref, path, param, workingMemory, cancelCb, user, outPath, outPathSize);

	if (res == 0x80101A09) {
		char *p = sceClibStrrchr(path, '/');
		if (p) {
			sceIoMkdir("ux0:download", 0006);

			char download_path[1024];
			sceClibSnprintf(download_path, sizeof(download_path), "ux0:download/%s", p+1);

			sceIoRemove(download_path);
			return sceIoRename(path, download_path);
		}
	}

	return res;
}

static int sceSysmoduleLoadModuleInternalPatched(SceUInt16 id) {
	int res = TAI_CONTINUE(int, sceSysmoduleLoadModuleInternalRef, id);

	if (res >= 0 && id == SCE_SYSMODULE_PHOTO_EXPORT) {
		hooks[4] = taiHookFunctionImport(&scePhotoExportFromFile2Ref, "SceShell", 0x79166BD9, 0x76640642, scePhotoExportFromFile2Patched);
	}

	return res;
}

static int sceSysmoduleUnloadModuleInternalPatched(SceUInt16 id) {
	int res = TAI_CONTINUE(int, sceSysmoduleUnloadModuleInternalRef, id);

	if (res >= 0 && id == SCE_SYSMODULE_PHOTO_EXPORT) {
		if (hooks[4] >= 0)
			taiHookRelease(hooks[4], scePhotoExportFromFile2Ref);
	}

	return res;
}

void _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize args, void *argp) {
	tai_module_info_t info;
	info.size = sizeof(info);
	if (taiGetModuleInfo("SceShell", &info) >= 0) {
		switch (info.module_nid) {
			case 0x0552F692: // retail 3.60 SceShell
			{
				hooks[0] = taiInjectData(info.modid, 0, 0x50A4A8, "GET", 4);
				hooks[1] = taiHookFunctionOffset(&GetFileTypeRef, info.modid, 0, 0x11B5E4, 1, GetFileTypePatched);
				break;
			}
		}

		hooks[2] = taiHookFunctionImport(&sceSysmoduleLoadModuleInternalRef, "SceShell", 0x03FCF19D, 0x2399BF45, sceSysmoduleLoadModuleInternalPatched);
		hooks[3] = taiHookFunctionImport(&sceSysmoduleUnloadModuleInternalRef, "SceShell", 0x03FCF19D, 0xFF206B19, sceSysmoduleUnloadModuleInternalPatched);

		if (hooks[2] < 0 && hooks[3] < 0) {
			hooks[4] = taiHookFunctionImport(&scePhotoExportFromFile2Ref, "SceShell", 0x79166BD9, 0x76640642, scePhotoExportFromFile2Patched);
		}
	}

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, void *argp) {
	if (hooks[4] >= 0)
		taiHookRelease(hooks[4], scePhotoExportFromFile2Ref);
	if (hooks[3] >= 0)
		taiHookRelease(hooks[3], sceSysmoduleUnloadModuleInternalRef);
	if (hooks[2] >= 0)
		taiHookRelease(hooks[2], sceSysmoduleLoadModuleInternalRef);
	if (hooks[1] >= 0)
		taiHookRelease(hooks[1], GetFileTypeRef);
	if (hooks[0] >= 0)
		taiInjectRelease(hooks[0]);

	return SCE_KERNEL_STOP_SUCCESS;
}