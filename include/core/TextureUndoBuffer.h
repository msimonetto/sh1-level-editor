#pragma once
#include "formats/TIMDecoder.h"
#include <deque>
#include <string>

// ---------------------------------------------------------------------------
// TextureSnapshot — state record of a texture editing operation
// ---------------------------------------------------------------------------
struct TextureSnapshot {
  DecodedTIM tim;
  int paletteIndex = 0;
  std::string description;
};

// ---------------------------------------------------------------------------
// TextureUndoBuffer — isolated undo/redo stack for a single TextureEditPanel
// ---------------------------------------------------------------------------
class TextureUndoBuffer {
public:
  explicit TextureUndoBuffer(int maxDepth = 50);

  // Push previous state before modification, clears redo stack
  void Push(const DecodedTIM &state, int paletteIdx, const std::string &desc);

  // Undo: pushes currentState onto redo stack, restores previous snapshot
  bool Undo(DecodedTIM &currentState, int &currentPalette, std::string &outDesc);

  // Redo: pushes currentState onto undo stack, restores next snapshot
  bool Redo(DecodedTIM &currentState, int &currentPalette, std::string &outDesc);

  bool CanUndo() const { return !m_undoStack.empty(); }
  bool CanRedo() const { return !m_redoStack.empty(); }

  const std::string &PeekUndoDesc() const;
  const std::string &PeekRedoDesc() const;

  void Clear();
  void SetMaxDepth(int depth) { m_maxDepth = depth; }
  int GetMaxDepth() const { return m_maxDepth; }

private:
  int m_maxDepth = 50;
  std::deque<TextureSnapshot> m_undoStack;
  std::deque<TextureSnapshot> m_redoStack;

  static const std::string s_emptyDesc;
};
