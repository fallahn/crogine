/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine - Zlib license.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#ifdef __ANDROID__

#include <stdio.h>

#include <crogine/android/Android.hpp>
#include <errno.h>
#include <android/asset_manager.h>

static int android_read(void* cookie, char* buf, int size) {
    return AAsset_read((AAsset*)cookie, buf, size);
}

static int android_write(void* cookie, const char* buf, int size) {
    return EACCES; // can't provide write access to the apk
}

static fpos_t android_seek(void* cookie, fpos_t offset, int whence) {
    return AAsset_seek((AAsset*)cookie, offset, whence);
}

static int android_close(void* cookie) {
    AAsset_close((AAsset*)cookie);
    return 0;
}

// must be established by someone else...
AAssetManager* android_asset_manager;
void android_fopen_set_asset_manager(AAssetManager* manager) {
    android_asset_manager = manager;
}

FILE* android_fopen(const char* fname, const char* mode) {
    if (mode[0] == 'w') return NULL;

    AAsset* asset = AAssetManager_open(android_asset_manager, fname, 0);
    if (!asset) return NULL;

    return funopen(asset, android_read, android_write, android_seek, android_close);
}

#endif //__ANDROID__