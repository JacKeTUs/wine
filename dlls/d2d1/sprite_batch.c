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

    if (!batch || !spriteCount || !destinationRects)
        return E_INVALIDARG;

    /* Expand storage if needed */
    UINT32 newSize = batch->sprite_count + spriteCount;
    if (newSize > batch->capacity)
    {
        UINT32 newCapacity = max(newSize, batch->capacity * 2);

        batch->destinationRects = realloc(batch->destinationRects, newCapacity * sizeof(D2D1_RECT_F));
        if (sourceRects) batch->sourceRects = realloc(batch->sourceRects, newCapacity * sizeof(D2D1_RECT_U));
        if (colors) batch->colors = realloc(batch->colors, newCapacity * sizeof(D2D1_COLOR_F));
        if (transforms) batch->transforms = realloc(batch->transforms, newCapacity * sizeof(D2D1_MATRIX_3X2_F));

        if (!batch->destinationRects || (sourceRects && !batch->sourceRects) ||
            (colors && !batch->colors) || (transforms && !batch->transforms))
        {
            WARN("Out of memory when realloc");
            return E_OUTOFMEMORY;
        }
            
        batch->capacity = newCapacity;
    }

    UINT32 startIndex = batch->sprite_count;

    /* Copy data */
    for (UINT32 i = 0; i < spriteCount; i++)
    {
        UINT32 spriteIdx = startIndex + i;

        batch->destinationRects[spriteIdx] =
            destinationRectanglesStride ? *(const D2D1_RECT_F *)((const char *)destinationRects + i * destinationRectanglesStride)
                                   : *destinationRects;

        if (sourceRects)
            batch->sourceRects[spriteIdx] =
                sourceRectanglesStride ? *(const D2D1_RECT_U *)((const char *)sourceRects + i * sourceRectanglesStride)
                                  : *sourceRects;

        if (colors)
            batch->colors[spriteIdx] =
                colorsStride ? *(const D2D1_COLOR_F *)((const char *)colors + i * colorsStride)
                             : *colors;

        if (transforms)
            batch->transforms[spriteIdx] =
                transformsStride ? *(const D2D1_MATRIX_3X2_F *)((const char *)transforms + i * transformsStride)
                                 : *transforms;
    }

    batch->sprite_count = newSize;
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
    
    /* Validation */
    if (!batch || startIndex + spriteCount > batch->sprite_count)
        return E_INVALIDARG;

    /* Allocate missing arrays if needed */
    if (destinationRects && !batch->destinationRects) {
        batch->destinationRects = calloc(batch->sprite_count, sizeof(D2D1_RECT_F));
        if (!batch->destinationRects)
            return E_OUTOFMEMORY;
    }

    if (sourceRects && !batch->sourceRects) {
        batch->sourceRects = calloc(batch->sprite_count, sizeof(D2D1_RECT_U));
        if (!batch->sourceRects)
            return E_OUTOFMEMORY;
    }

    if (colors && !batch->colors) {
        batch->colors = calloc(batch->sprite_count, sizeof(D2D1_COLOR_F));
        if (!batch->colors)
            return E_OUTOFMEMORY;
    }

    if (transforms && !batch->transforms) {
        batch->transforms = calloc(batch->sprite_count, sizeof(D2D1_MATRIX_3X2_F));
        if (!batch->transforms)
            return E_OUTOFMEMORY;
    }

    /* Update only specified sprites */
    for (UINT32 i = 0; i < spriteCount; i++)
    {
        UINT32 spriteIdx = startIndex + i;

        /* Update destination rects if provided */
        if (destinationRects)
            batch->destinationRects[spriteIdx] =
                destinationRectanglesStride ? *(const D2D1_RECT_F *)((const char *)destinationRects + i * destinationRectanglesStride)
                                       : *destinationRects;

        /* Update source rects if provided */
        if (sourceRects)
            batch->sourceRects[spriteIdx] =
                sourceRectanglesStride ? *(const D2D1_RECT_U *)((const char *)sourceRects + i * sourceRectanglesStride)
                                       : *sourceRects;

        /* Update colors if provided */
        if (colors)
            batch->colors[spriteIdx] =
                colorsStride ? *(const D2D1_COLOR_F *)((const char *)colors + i * colorsStride)
                             : *colors;

        /* Update transforms if provided */
        if (transforms)
            batch->transforms[spriteIdx] =
                transformsStride ? *(const D2D1_MATRIX_3X2_F *)((const char *)transforms + i * transformsStride)
                                 : *transforms;
    }

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_sprite_batch_GetSprites(ID2D1SpriteBatch *iface, UINT32 start_index, UINT32 count,
        D2D1_RECT_F *destination_rects, D2D1_RECT_U *source_rects,
        D2D1_COLOR_F *colors, D2D1_MATRIX_3X2_F *transforms)
{
    struct d2d_sprite_batch *sprite_batch = impl_from_ID2D1SpriteBatch(iface);
    
    TRACE("iface %p, %d, %d, %p, %p, %p, %p\n", iface, start_index, count, destination_rects, source_rects, colors, transforms);
    
    if (!destination_rects || start_index + count > sprite_batch->sprite_count)
        return E_INVALIDARG;

    memcpy(destination_rects, sprite_batch->destinationRects + start_index, count * sizeof(D2D1_RECT_F));
    if (source_rects)
        memcpy(source_rects, sprite_batch->sourceRects + start_index, count * sizeof(D2D1_RECT_U));
    if (colors)
        memcpy(colors, sprite_batch->colors + start_index, count * sizeof(D2D1_COLOR_F));
    if (transforms)
        memcpy(transforms, sprite_batch->transforms + start_index, count * sizeof(D2D1_MATRIX_3X2_F));

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
    TRACE("iface %p\n", iface);
    TRACE("batch %p\n", batch);
    if (!batch) {
        WARN("NULL batch here\n");
        return;
    }

    /* Free allocated memory */
    if (batch->destinationRects) free(batch->destinationRects);
    if (batch->sourceRects) free(batch->sourceRects);
    if (batch->colors) free(batch->colors);
    if (batch->transforms) free(batch->transforms);

    /* Set pointers to NULL to prevent dangling access */
    batch->destinationRects = NULL;
    batch->sourceRects = NULL;
    batch->colors = NULL;
    batch->transforms = NULL;

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