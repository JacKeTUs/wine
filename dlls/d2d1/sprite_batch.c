/*
 * Copyright 2025 Makarenko Oleg
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "d2d1_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

static inline struct d2d_sprite_batch *impl_from_ID2D1SpriteBatch(ID2D1SpriteBatch *iface)
{
    TRACE("iface %p\n", iface);
    return CONTAINING_RECORD(iface, struct d2d_sprite_batch, ID2D1SpriteBatch_iface);
}


static HRESULT STDMETHODCALLTYPE d2d_sprite_batch_QueryInterface(ID2D1SpriteBatch *iface, REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_ID2D1SpriteBatch)
            || IsEqualGUID(iid, &IID_ID2D1Resource)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        ID2D1SpriteBatch_AddRef(iface);
        *out = iface;
        return S_OK;
    }
    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_sprite_batch_AddRef(ID2D1SpriteBatch *iface)
{
    struct d2d_sprite_batch *sprite_batch = impl_from_ID2D1SpriteBatch(iface);
    ULONG refcount = InterlockedIncrement(&sprite_batch->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE d2d_sprite_batch_Release(ID2D1SpriteBatch *iface)
{
    struct d2d_sprite_batch *sprite_batch = impl_from_ID2D1SpriteBatch(iface);
    ULONG refcount = InterlockedDecrement(&sprite_batch->refcount);

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount) {
        ID2D1Factory_Release(sprite_batch->factory);
        free(sprite_batch);
    }

    return refcount;
}

static void STDMETHODCALLTYPE d2d_sprite_batch_GetFactory(ID2D1SpriteBatch *iface, ID2D1Factory **factory)
{
    struct d2d_sprite_batch *sprite_batch = impl_from_ID2D1SpriteBatch(iface);

    TRACE("iface %p, factory %p.\n", iface, factory);

    ID2D1Factory_AddRef(*factory = sprite_batch->factory);
}

static HRESULT STDMETHODCALLTYPE d2d_sprite_batch_AddSprites(ID2D1SpriteBatch *iface, UINT32 spriteCount,
        const D2D1_RECT_F *destinationRects, const D2D1_RECT_U *sourceRects,
        const D2D1_COLOR_F *colors, const D2D1_MATRIX_3X2_F *transforms,
            UINT32 destinationRectanglesStride,
            UINT32 sourceRectanglesStride,
            UINT32 colorsStride,
            UINT32 transformsStride)
{
    struct d2d_sprite_batch *batch = impl_from_ID2D1SpriteBatch(iface);

    TRACE("iface %p, batch %p, spriteCount %d, d %p, s %p, c %p, t %p, %d, %d, %d, %d\n", iface, batch, spriteCount, destinationRects, \
            sourceRects, colors, transforms, destinationRectanglesStride, sourceRectanglesStride, colorsStride, transformsStride);

    if (!batch || !spriteCount || !destinationRects) {
        WARN("spriteCount or destrect null");
        return E_INVALIDARG;
    }

    /* Expand storage if needed */
    UINT32 new_sprite_count = batch->sprite_count + spriteCount;
    
    struct d2d_sprite* new_sprites = realloc(batch->sprites, new_sprite_count*sizeof(struct d2d_sprite));
    if (!new_sprites)
        return E_OUTOFMEMORY;
    batch->sprites = new_sprites;
    
    // "Zero" new sprite
    D2D1_RECT_U inf = {0,0,UINT_MAX,UINT_MAX};

    D2D1_COLOR_F color1={1.0f,1.0f,1.0f,1.0f};
    D2D1_MATRIX_3X2_F identity =
                                {{{
                                    1.0f, 0.0f,
                                    0.0f, 1.0f,
                                    0.0f, 0.0f,
                                }}};
                                
    /* Not efficient in memory, but most simple approach for now */
    for (UINT32 i = batch->sprite_count; i < new_sprite_count; i++) {

        //batch->sprites[i].sourceRect = inf;
        //batch->sprites[i].color = color1;
        //batch->sprites[i].transform = identity;

        batch->sprites[i].destinationRect = *(const D2D1_RECT_F *)((const char *)destinationRects + 
                                (i - batch->sprite_count) * destinationRectanglesStride);
        if (sourceRects)
            batch->sprites[i].sourceRect = *(const D2D1_RECT_U *)((const char *)sourceRects + 
                                (i - batch->sprite_count) * sourceRectanglesStride);
        else 
            batch->sprites[i].sourceRect = inf;

        if (colors)
            batch->sprites[i].color = *(const D2D1_COLOR_F *)((const char *)colors + 
                                (i - batch->sprite_count) * colorsStride);
        else 
            batch->sprites[i].color = color1;

        if (transforms)
            batch->sprites[i].transform = *(const D2D1_MATRIX_3X2_F *)((const char *)transforms + 
                                (i - batch->sprite_count) * transformsStride);
        else
            batch->sprites[i].transform = identity;
            
    }
    
    batch->sprite_count = new_sprite_count;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_sprite_batch_SetSprites(ID2D1SpriteBatch *iface, UINT32 startIndex, UINT32 spriteCount,
        const D2D1_RECT_F *destinationRects, const D2D1_RECT_U *sourceRects,
        const D2D1_COLOR_F *colors, const D2D1_MATRIX_3X2_F *transforms,
            UINT32 destinationRectanglesStride,
            UINT32 sourceRectanglesStride,
            UINT32 colorsStride,
            UINT32 transformsStride)
{
    struct d2d_sprite_batch *batch = impl_from_ID2D1SpriteBatch(iface);
    
    TRACE("iface %p, batch %p, start %d, count %d, d %p, s %p, c %p, t %p, %d, %d, %d, %d\n", iface, batch, startIndex, spriteCount, destinationRects, \
            sourceRects, colors, transforms, destinationRectanglesStride, sourceRectanglesStride, colorsStride, transformsStride);
    
    if (!batch || startIndex + spriteCount > batch->sprite_count) {
        WARN("wrong size");
        return E_INVALIDARG;
    }

    /* Update only specified sprites */
    for (UINT32 i = startIndex; i < spriteCount; i++)
    {
        UINT32 spriteIdx = startIndex + i;

        if (destinationRects)
            batch->sprites[i].destinationRect = *(const D2D1_RECT_F *)((const char *)destinationRects + 
                                (i - batch->sprite_count) * destinationRectanglesStride);
        if (sourceRects)
            batch->sprites[i].sourceRect = *(const D2D1_RECT_U *)((const char *)sourceRects + 
                                (i - batch->sprite_count) * sourceRectanglesStride);
        if (colors)
            batch->sprites[i].color = *(const D2D1_COLOR_F *)((const char *)colors + 
                                (i - batch->sprite_count) * colorsStride);
        if (transforms)
            batch->sprites[i].transform = *(const D2D1_MATRIX_3X2_F *)((const char *)transforms + 
                                (i - batch->sprite_count) * transformsStride);
    }

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_sprite_batch_GetSprites(ID2D1SpriteBatch *iface, UINT32 startIndex, UINT32 spriteCount,
        D2D1_RECT_F *destinationRects, D2D1_RECT_U *sourceRects,
        D2D1_COLOR_F *colors, D2D1_MATRIX_3X2_F *transforms)
{
    struct d2d_sprite_batch *batch = impl_from_ID2D1SpriteBatch(iface);
    
    TRACE("iface %p, %d, %d, %p, %p, %p, %p\n", iface, startIndex, spriteCount, destinationRects, sourceRects, colors, transforms);
    
    if (startIndex + spriteCount > batch->sprite_count) {
        WARN("wrong size");
        return E_INVALIDARG;
    }

    /* Get only specified sprites */
    for (UINT32 i = startIndex; i < spriteCount; i++)
    {
        if (destinationRects)
            destinationRects[i-startIndex] = batch->sprites[i].destinationRect;
        if (sourceRects)
            sourceRects[i-startIndex] = batch->sprites[i].sourceRect;
        if (colors)
            colors[i-startIndex] = batch->sprites[i].color;
        if (transforms)
            transforms[i-startIndex] = batch->sprites[i].transform;
    }
    return S_OK;
}

static UINT32 STDMETHODCALLTYPE d2d_sprite_batch_GetSpriteCount(ID2D1SpriteBatch *iface)
{
    struct d2d_sprite_batch *sprite_batch = impl_from_ID2D1SpriteBatch(iface);
    TRACE("iface %p\n", iface);
    TRACE("batch %p\n", sprite_batch);
    if (!sprite_batch) {
        WARN("NULL batch here\n");
        return 0;
    }
    return sprite_batch->sprite_count;
}

static void STDMETHODCALLTYPE d2d_sprite_batch_Clear(ID2D1SpriteBatch *iface)
{
    struct d2d_sprite_batch *batch = impl_from_ID2D1SpriteBatch(iface);
    TRACE("iface %p batch %p\n", iface, batch);
    if (!batch) {
        WARN("NULL batch here\n");
        return;
    }
    /* Free allocated memory */
    free(batch->sprites);
    batch->sprite_count = 0;
}


static const ID2D1SpriteBatchVtbl d2d_sprite_batch_vtbl =
{
    d2d_sprite_batch_QueryInterface,
    d2d_sprite_batch_AddRef,
    d2d_sprite_batch_Release,
    d2d_sprite_batch_GetFactory,
    d2d_sprite_batch_AddSprites,
    d2d_sprite_batch_SetSprites,
    d2d_sprite_batch_GetSprites,
    d2d_sprite_batch_GetSpriteCount,
    d2d_sprite_batch_Clear,
    
};

HRESULT d2d_sprite_batch_create(ID2D1Factory *factory, struct d2d_sprite_batch **sprite_batch)
{
    if (!(*sprite_batch = calloc(1, sizeof(**sprite_batch))))
        return E_OUTOFMEMORY;

    (*sprite_batch)->ID2D1SpriteBatch_iface.lpVtbl = &d2d_sprite_batch_vtbl;
    (*sprite_batch)->refcount = 1;
    (*sprite_batch)->sprite_count = 0;
    (*sprite_batch)->sprites = NULL;
    ID2D1Factory_AddRef((*sprite_batch)->factory = factory);

    TRACE("Created sprite batch %p.\n", *sprite_batch);

    return S_OK;
}

struct d2d_sprite_batch *unsafe_impl_from_ID2D1SpriteBatch(ID2D1SpriteBatch *iface)
{
    TRACE("iface %p\n", iface);
    if (!iface)
        return NULL;
    assert(iface->lpVtbl == (ID2D1SpriteBatchVtbl *)&d2d_sprite_batch_vtbl);
    return CONTAINING_RECORD(iface, struct d2d_sprite_batch, ID2D1SpriteBatch_iface);
}