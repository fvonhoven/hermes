/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the LICENSE
 * file in the root directory of this source tree.
 */
#include "hermes/VM/GCGeneration.h"

#include "hermes/Public/GCConfig.h"
#include "hermes/VM/GCCell-inline.h"

#include <memory>

namespace hermes {
namespace vm {

GCGeneration::GCGeneration(GenGC *gc)
    : trueAllocContext_(&allocContext_), gc_(gc) {}

GCGeneration::~GCGeneration() = default;

void GCGeneration::finalizeUnreachableObjects() {
  numFinalizedObjects_ = 0;
  for (gcheapsize_t i = 0; i < cellsWithFinalizers().size(); i++) {
    GCCell *cell = cellsWithFinalizers().at(i);
    if (!AlignedHeapSegment::getCellMarkBit(cell)) {
      cell->getVT()->finalize(cell, gc_);
      numFinalizedObjects_++;
      continue;
    }
    cellsWithFinalizers()[i - numFinalizedObjects_] = cell;
  }
  cellsWithFinalizers().resize(
      cellsWithFinalizers().size() - numFinalizedObjects_);
}

void GCGeneration::updateFinalizableCellListReferences() {
  for (auto &cell : cellsWithFinalizers()) {
    cell = cell->getForwardingPointer();
  }
}

uint64_t GCGeneration::extSizeFromFinalizerList() const {
  uint64_t extSize = 0;
  for (const auto cell : cellsWithFinalizers()) {
    extSize += cell->externalMemorySize();
  }
  return extSize;
}

#ifdef HERMES_SLOW_DEBUG
void GCGeneration::checkFinalizableObjectsListWellFormed() const {
  for (GCCell *cell : cellsWithFinalizers()) {
    assert(
        dbgContains(cell) && "finalizer list should contain objects in gen.");
    assert(cell->isValid() && "finalizer list should contain valid objects.");
  }
}
#endif

void swap(GCGeneration::AllocContext &a, GCGeneration::AllocContext &b) {
  using std::swap;
  swap(a.activeSegment, b.activeSegment);
  swap(a.cellsWithFinalizers, b.cellsWithFinalizers);
#ifndef NDEBUG
  swap(a.numAllocatedObjects, b.numAllocatedObjects);
#endif
}

void GCGeneration::exchangeActiveSegment(
    AlignedHeapSegment &&newSeg,
    AlignedHeapSegment *oldSegSlot) {
  if (LLVM_LIKELY(oldSegSlot)) {
    *oldSegSlot = std::move(activeSegment());
    gc_->segmentMoved(oldSegSlot);
  }

  activeSegmentRaw() = std::move(newSeg);
  gc_->segmentMoved(&activeSegmentRaw());
}

} // namespace vm
} // namespace hermes
