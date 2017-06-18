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
static tai_hook_ref_t scePhotoExportFromFile2Ref;
static tai_hook_ref_t sceSysmoduleLoadModuleInternalRef;
static tai_hook_ref_t sceSysmoduleUnloadModuleInternalRef;

static SceUID hooks[4];

int GetFileTypePatched(int unk, int *type, char **filename, char **mime_type) {
	int res = TAI_CONTINUE(int, GetFileTypeRef, unk, type, filename, mime_type);

	if (res < 0) {
		*type = 1; // Type photo
		return 0;
	}

	return res;
}

int scePhotoExportFromFile2Patched(const char *path, const PhotoExportParam *param, void *workingMemory, void *cancelCb, void *user, char *outPath, SceSize outPathSize) {
	int res = TAI_CONTINUE(int, scePhotoExportFromFile2Ref, path, param, workingMemory, cancelCb, user, outPath, outPathSize);

	if (res == 0x80101A09) {
		char *p = sceClibStrrchr(path, '/');
		if (p) {
			sceIoMkdir("ux0:downloads", 0777);

			char download_path[1024];
			sceClibSnprintf(download_path, sizeof(download_path), "ux0:downloads/%s", p+1);

			sceIoRemove(download_path);
			return sceIoRename(path, download_path);
		}
	}

	return res;
}

int sceSysmoduleLoadModuleInternalPatched(SceUInt16 id) {
	int res = TAI_CONTINUE(int, sceSysmoduleLoadModuleInternalRef, id);

	if (res >= 0 && id == SCE_SYSMODULE_PHOTO_EXPORT) {
		hooks[3] = taiHookFunctionImport(&scePhotoExportFromFile2Ref, "SceShell", TAI_ANY_LIBRARY, 0x76640642, scePhotoExportFromFile2Patched);
	}

	return res;
}

int sceSysmoduleUnloadModuleInternalPatched(SceUInt16 id) {
	int res = TAI_CONTINUE(int, sceSysmoduleUnloadModuleInternalRef, id);

	if (res >= 0 && id == SCE_SYSMODULE_PHOTO_EXPORT) {
		if (hooks[3] >= 0)
			taiHookRelease(hooks[3], scePhotoExportFromFile2Ref);
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
				hooks[0] = taiHookFunctionOffset(&GetFileTypeRef, info.modid, 0, 0x11B5E4, 1, GetFileTypePatched);
				break;
		}

		hooks[1] = taiHookFunctionImport(&sceSysmoduleLoadModuleInternalRef, "SceShell", TAI_ANY_LIBRARY, 0x2399BF45, sceSysmoduleLoadModuleInternalPatched);
		hooks[2] = taiHookFunctionImport(&sceSysmoduleUnloadModuleInternalRef, "SceShell", TAI_ANY_LIBRARY, 0xFF206B19, sceSysmoduleUnloadModuleInternalPatched);

		if (hooks[1] < 0 && hooks[2] < 0) {
			hooks[3] = taiHookFunctionImport(&scePhotoExportFromFile2Ref, "SceShell", TAI_ANY_LIBRARY, 0x76640642, scePhotoExportFromFile2Patched);
		}
	}

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, void *argp) {
	if (hooks[2] >= 0)
		taiHookRelease(hooks[2], sceSysmoduleUnloadModuleInternalRef);
	if (hooks[1] >= 0)
		taiHookRelease(hooks[1], sceSysmoduleLoadModuleInternalRef);
	if (hooks[0] >= 0)
		taiHookRelease(hooks[0], GetFileTypeRef);

	return SCE_KERNEL_STOP_SUCCESS;
}