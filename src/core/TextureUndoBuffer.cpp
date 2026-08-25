#include "core/TextureUndoBuffer.h"

const std::string TextureUndoBuffer::s_emptyDesc = "";

TextureUndoBuffer::TextureUndoBuffer(int maxDepth) : m_maxDepth(maxDepth) {}

void TextureUndoBuffer::Push(const DecodedTIM &state, int paletteIdx,
                             const std::string &desc) {
  TextureSnapshot snap;
  snap.tim = state;
  snap.paletteIndex = paletteIdx;
  snap.description = desc;

  m_undoStack.push_back(std::move(snap));
  while ((int)m_undoStack.size() > m_maxDepth) {
    m_undoStack.pop_front();
  }
  m_redoStack.clear();
}

bool TextureUndoBuffer::Undo(DecodedTIM &currentState, int &currentPalette,
                             std::string &outDesc) {
  if (m_undoStack.empty())
    return false;

  // Save current state into redo stack
  TextureSnapshot redoSnap;
  redoSnap.tim = currentState;
  redoSnap.paletteIndex = currentPalette;
  redoSnap.description = m_undoStack.back().description;
  m_redoStack.push_back(std::move(redoSnap));
  while ((int)m_redoStack.size() > m_maxDepth) {
    m_redoStack.pop_front();
  }

  // Restore snapshot from undo stack
  TextureSnapshot snap = std::move(m_undoStack.back());
  m_undoStack.pop_back();

  outDesc = snap.description;
  currentState = std::move(snap.tim);
  currentPalette = snap.paletteIndex;
  return true;
}

bool TextureUndoBuffer::Redo(DecodedTIM &currentState, int &currentPalette,
                             std::string &outDesc) {
  if (m_redoStack.empty())
    return false;

  // Save current state into undo stack
  TextureSnapshot undoSnap;
  undoSnap.tim = currentState;
  undoSnap.paletteIndex = currentPalette;
  undoSnap.description = m_redoStack.back().description;
  m_undoStack.push_back(std::move(undoSnap));
  while ((int)m_undoStack.size() > m_maxDepth) {
    m_undoStack.pop_front();
  }

  // Restore snapshot from redo stack
  TextureSnapshot snap = std::move(m_redoStack.back());
  m_redoStack.pop_back();

  outDesc = snap.description;
  currentState = std::move(snap.tim);
  currentPalette = snap.paletteIndex;
  return true;
}

const std::string &TextureUndoBuffer::PeekUndoDesc() const {
  if (m_undoStack.empty())
    return s_emptyDesc;
  return m_undoStack.back().description;
}

const std::string &TextureUndoBuffer::PeekRedoDesc() const {
  if (m_redoStack.empty())
    return s_emptyDesc;
  return m_redoStack.back().description;
}

void TextureUndoBuffer::Clear() {
  m_undoStack.clear();
  m_redoStack.clear();
}
