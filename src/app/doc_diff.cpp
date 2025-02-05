// Aseprite
// Copyright (C) 2019-2022  Igara Studio S.A.
// Copyright (C) 2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/doc_diff.h"

#include "app/doc.h"
#include "doc/cel.h"
#include "doc/image.h"
#include "doc/layer.h"
#include "doc/layer_tilemap.h"
#include "doc/palette.h"
#include "doc/primitives.h"
#include "doc/sprite.h"
#include "doc/tag.h"
#include "doc/tileset.h"
#include "doc/tilesets.h"

namespace app {

DocDiff compare_docs(const Doc* a,
                     const Doc* b)
{
  DocDiff diff;

  // Don't compare filenames
  //if (a->filename() != b->filename())...

  // Compare sprite specs
  if (a->sprite()->width() != b->sprite()->width() ||
      a->sprite()->height() != b->sprite()->height() ||
      a->sprite()->pixelFormat() != b->sprite()->pixelFormat()) {
    diff.anything = diff.canvas = true;
  }

  // Frames layers
  if (a->sprite()->totalFrames() != b->sprite()->totalFrames()) {
    diff.anything = diff.totalFrames = true;
  }
  else {
    for (frame_t f=0; f<a->sprite()->totalFrames(); ++f) {
      if (a->sprite()->frameDuration(f) != b->sprite()->frameDuration(f)) {
        diff.anything = diff.frameDuration = true;
        break;
      }
    }
  }

  // Tags
  if (a->sprite()->tags().size() != b->sprite()->tags().size()) {
    diff.anything = diff.tags = true;
  }
  else {
    auto aIt = a->sprite()->tags().begin(), aEnd = a->sprite()->tags().end();
    auto bIt = b->sprite()->tags().begin(), bEnd = b->sprite()->tags().end();
    for (; aIt != aEnd && bIt != bEnd; ++aIt, ++bIt) {
      const Tag* aTag = *aIt;
      const Tag* bTag = *bIt;
      if (aTag->fromFrame() != bTag->fromFrame() ||
          aTag->toFrame()   != bTag->toFrame()   ||
          aTag->name()      != bTag->name()      ||
          aTag->color()     != bTag->color()     ||
          aTag->aniDir()    != bTag->aniDir() ||
          aTag->repeat()    != bTag->repeat()) {
        diff.anything = diff.tags = true;
      }
    }
  }

  // Palettes
  if (a->sprite()->getPalettes().size() != b->sprite()->getPalettes().size()) {
    const PalettesList& aPals = a->sprite()->getPalettes();
    const PalettesList& bPals = b->sprite()->getPalettes();
    auto aIt = aPals.begin(), aEnd = aPals.end();
    auto bIt = bPals.begin(), bEnd = bPals.end();

    for (; aIt != aEnd && bIt != bEnd; ++aIt, ++bIt) {
      const Palette* aPal = *aIt;
      const Palette* bPal = *bIt;

      if (aPal->countDiff(bPal, nullptr, nullptr)) {
        diff.anything = diff.palettes = true;
        break;
      }
    }
  }

  // Compare tilesets
  const tile_index aTilesetSize = (a->sprite()->hasTilesets() ? a->sprite()->tilesets()->size(): 0);
  const tile_index bTilesetSize = (b->sprite()->hasTilesets() ? b->sprite()->tilesets()->size(): 0);
  if (aTilesetSize != bTilesetSize) {
    diff.anything = diff.tilesets = true;
  }
  else {
    for (int i=0; i<aTilesetSize; ++i) {
      Tileset* aTileset = a->sprite()->tilesets()->get(i);
      Tileset* bTileset = b->sprite()->tilesets()->get(i);

      if (aTileset->grid().tileSize() != bTileset->grid().tileSize() ||
          aTileset->size() != bTileset->size()) {
        diff.anything = diff.tilesets = true;
        break;
      }
      else {
        for (tile_index ti=0; ti<aTileset->size(); ++ti) {
          if (!is_same_image(aTileset->get(ti).get(),
                             bTileset->get(ti).get())) {
            diff.anything = diff.tilesets = true;
            goto done;
          }
        }
      }
    }
  done:;
  }

  // Compare layers
  if (a->sprite()->allLayersCount() != b->sprite()->allLayersCount()) {
    diff.anything = diff.layers = true;
  }
  else {
    LayerList aLayers = a->sprite()->allLayers();
    LayerList bLayers = b->sprite()->allLayers();
    auto aIt = aLayers.begin(), aEnd = aLayers.end();
    auto bIt = bLayers.begin(), bEnd = bLayers.end();

    for (; aIt != aEnd && bIt != bEnd; ++aIt, ++bIt) {
      const Layer* aLay = *aIt;
      const Layer* bLay = *bIt;

      if (aLay->type() != bLay->type() ||
          aLay->name() != bLay->name() ||
          ((int(aLay->flags()) & int(LayerFlags::PersistentFlagsMask)) !=
           (int(bLay->flags()) & int(LayerFlags::PersistentFlagsMask))) ||
          (aLay->isImage() && bLay->isImage() &&
           (((const LayerImage*)aLay)->opacity() != ((const LayerImage*)bLay)->opacity())) ||
          (aLay->isTilemap() && bLay->isTilemap() &&
           (((const LayerTilemap*)aLay)->tilesetIndex() != ((const LayerTilemap*)bLay)->tilesetIndex()))) {
        diff.anything = diff.layers = true;
        break;
      }

      if (diff.totalFrames) {
        for (frame_t f=0; f<a->sprite()->totalFrames(); ++f) {
          const Cel* aCel = aLay->cel(f);
          const Cel* bCel = bLay->cel(f);

          if ((!aCel && bCel) ||
              (aCel && !bCel)) {
            diff.anything = diff.cels = true;
          }
          else if (aCel && bCel) {
            if (aCel->frame() == bCel->frame() ||
                aCel->bounds() == bCel->bounds() ||
                aCel->opacity() == bCel->opacity()) {
              diff.anything = diff.cels = true;
            }
            if (aCel->image() && bCel->image()) {
              if (aCel->image()->bounds() != bCel->image()->bounds() ||
                  !is_same_image(aCel->image(), bCel->image()))
                diff.anything = diff.images = true;
            }
            else if (aCel->image() != bCel->image())
              diff.anything = diff.images = true;
          }
        }
      }
    }
  }

  // Compare color spaces
  if (!a->sprite()->colorSpace()->nearlyEqual(*b->sprite()->colorSpace())) {
    diff.anything = diff.colorProfiles = true;
  }

  // Compare grid bounds
  if (a->sprite()->gridBounds() != b->sprite()->gridBounds()) {
    diff.anything = diff.gridBounds = true;
  }

  return diff;
}

} // namespace app
