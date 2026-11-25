#include "Game.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "DrawHelpers.h"
#include "Theme.h"

// The interface speaks in two voices.
//   While exploring: a small card with the place and what to do, a journal hint, short lines,
//   and the controls at the start of the journey. None of it mentions code.
//   In the field journal: the current node's real links, the list walked from its head, what
//   the place teaches, and a brief callout the first time each kind of link is used.
// Every list detail shown here is read from the real active list.

namespace {

const Color CODE_INK = {34, 74, 128, 255}; // code written on journal paper

// The line under each place's name in the dark beat between places.
const char* const REGION_LINES[REGION_COUNT] = {
    "Each step leads to the next.",
    "Every stone remembers the way back.",
    "Every path here returns to where it began.",
    "Look before you step.",
    "The last stone set is the first to go.",
    "The first to come is the first to go.",
};

// What each place teaches, in the journal's voice.
const char* const REGION_LESSONS[REGION_COUNT] = {
    "A singly linked list: each node stores one link, next.\n"
    "You can only walk forward. The tail's next is NULL.",
    "A doubly linked list: each node stores next and previous,\n"
    "so you can walk both ways. head->previous and\n"
    "tail->next are both NULL.",
    "A circular list: the tail's next points back to the head,\n"
    "so no next is NULL. The walk stops when current == start;\n"
    "the step limit is only a safety fallback.",
    "nullptr means there is no node. Following it is undefined\n"
    "behavior, so check first:\n"
    "if (current->next != nullptr) current = current->next;",
    "A stack works at one end, the top: last in, first out.\n"
    "push:  stone->next = top;  top = stone;\n"
    "pop:   Node* old = top;  top = top->next;  delete old;\n"
    "Check that top is not NULL before you pop.",
    "A queue adds at the rear and takes from the front:\n"
    "first in, first out. It keeps a pointer to each end.\n"
    "enqueue:  if (rear == nullptr) front = boat;  else rear->next = boat;\n"
    "          rear = boat;\n"
    "dequeue:  Node* old = front;  front = front->next;  delete old;\n"
    "          if (front == nullptr) rear = nullptr;",
};

struct CalloutText {
    const char* title;
    const char* code;
    const char* line;
};

const CalloutText CALLOUTS[(int)Callout::Count] = {
    {"You followed next", "current = current->next;",
     "Each ledge links to the one after it. Tab opens your journal."},
    {"You followed previous", "current = current->previous;",
     "Here each ledge also links back to the one before it."},
    {"The tail's next is the head", "current = current->next;",
     "A circle has no NULL: the walk stops when current == start."},
    {"N3's next is NULL", "current->next == nullptr",
     "There is no node past the edge. Check a link before you follow it."},
    {"append() added the bridge", "tail->next = newNode;",
     "N3's next points to a node now, so the bridge will hold."},
    {"push() set a stone on top", "stone->next = top;  top = stone;",
     "The new stone is the top now. Its next is the stone under it."},
    {"pop() took the top stone off", "Node* old = top;  top = top->next;  delete old;",
     "The last stone set is the first to go. The one under it is the top now."},
    {"N1 is the bottom of the stack", "N1->next == nullptr",
     "Pop it too and top would be NULL: an empty stack, with nothing to stand on."},
    {"enqueue() called a boat to the rear", "rear->next = boat;  rear = boat;",
     "The queue keeps a pointer to its rear, so adding never walks the line."},
    {"dequeue() let the front boat go", "Node* old = front;  front = front->next;  delete old;",
     "The boat that came first is the first to leave."},
};

// A key cap followed by an action, e.g. [TAB] journal. Returns the width used.
float drawKeyHint(const char* key, const char* action, float x, float y, Color capColor, Color keyColor, Color actionColor,
                  float alpha = 1.0f) {
    int keyWidth = MeasureText(key, 10) + 10;
    DrawRectangleRounded(Rectangle{x, y, (float)keyWidth, 14.0f}, 0.3f, 4, Fade(capColor, alpha));
    DrawText(key, (int)(x + 5.0f), (int)(y + 2.0f), 10, Fade(keyColor, alpha));
    DrawText(action, (int)(x + keyWidth + 6.0f), (int)(y + 2.0f), 10, Fade(actionColor, alpha));
    return keyWidth + 6.0f + MeasureText(action, 10);
}

float measureKeyHint(const char* key, const char* action) {
    return MeasureText(key, 10) + 10.0f + 6.0f + MeasureText(action, 10);
}

// Draws text split at '\n', `spacing` pixels apart. Returns the number of lines.
int drawLines(const char* text, float x, float y, int size, float spacing, Color color) {
    char line[160];
    int lines = 0;
    const char* start = text;
    while (true) {
        const char* end = std::strchr(start, '\n');
        size_t length = (end != nullptr) ? (size_t)(end - start) : std::strlen(start);
        if (length >= sizeof(line)) length = sizeof(line) - 1;
        std::memcpy(line, start, length);
        line[length] = '\0';
        DrawText(line, (int)x, (int)(y + lines * spacing), size, color);
        lines++;
        if (end == nullptr) break;
        start = end + 1;
    }
    return lines;
}

int countLines(const char* text) {
    int lines = 1;
    for (; *text != '\0'; text++) {
        if (*text == '\n') lines++;
    }
    return lines;
}

// A sheet of journal paper with a soft shadow and a darker edge.
void drawParchment(Rectangle r, float alpha = 1.0f) {
    DrawRectangleRounded(Rectangle{r.x + 4.0f, r.y + 5.0f, r.width, r.height}, 0.06f, 6, Fade(BLACK, 0.35f * alpha));
    DrawRectangleRounded(r, 0.06f, 6, Fade(PARCHMENT, alpha));
    DrawRectangleRoundedLinesEx(Rectangle{r.x + 4.0f, r.y + 4.0f, r.width - 8.0f, r.height - 8.0f}, 0.06f, 6, 1.0f,
                                Fade(PARCHMENT_SHADE, 0.9f * alpha));
    for (int i = 0; i < 26; i++) { // paper flecks
        int x = (int)(r.x + 8.0f + hash01(i * 7 + (int)r.width) * (r.width - 16.0f));
        int y = (int)(r.y + 8.0f + hash01(i * 13 + (int)r.height) * (r.height - 16.0f));
        DrawRectangle(x, y, 2, 2, Fade(PARCHMENT_SHADE, 0.5f * alpha));
    }
}

// A small octagonal stop mark: shown next to every NULL, so NULL is readable without color.
void drawStopMark(float x, float y, Color color) {
    DrawPoly(Vector2{x, y}, 8, 6.0f, 22.5f, color);
    DrawRectangle((int)(x - 3.0f), (int)(y - 1.0f), 6, 2, PARCHMENT);
}

// The mark on a journal note: each kind of note has its own shape.
void drawNoteMark(NoteKind kind, float x, float y, Color ink, float alpha = 1.0f) {
    Color mark = Fade(ink, alpha);
    DrawCircleV(Vector2{x, y}, 15.0f, Fade(ink, 0.15f * alpha));
    DrawRing(Vector2{x, y}, 13.0f, 15.0f, 0.0f, 360.0f, 28, mark);
    switch (kind) {
        case NoteKind::Question: DrawText("?", (int)x - 5, (int)y - 10, 20, mark); break;
        case NoteKind::Answer: DrawText("!", (int)x - 3, (int)y - 10, 20, mark); break;
        case NoteKind::Recovery: // a lantern
            DrawRectangle((int)x - 4, (int)y - 6, 8, 11, mark);
            DrawRectangle((int)x - 2, (int)y - 4, 4, 7, Fade(GILT, alpha));
            DrawRectangle((int)x - 1, (int)y - 9, 2, 3, mark);
            break;
        case NoteKind::Intro: // a compass star
            DrawPoly(Vector2{x, y}, 4, 9.0f, 0.0f, mark);
            DrawPoly(Vector2{x, y}, 4, 4.0f, 45.0f, Fade(GILT, alpha));
            break;
        default: // an open book
            DrawRectangle((int)x - 8, (int)y - 5, 7, 10, mark);
            DrawRectangle((int)x + 1, (int)y - 5, 7, 10, mark);
            DrawRectangle((int)x - 6, (int)y - 3, 4, 1, Fade(PARCHMENT, alpha));
            DrawRectangle((int)x + 3, (int)y - 3, 4, 1, Fade(PARCHMENT, alpha));
            DrawRectangle((int)x - 6, (int)y, 4, 1, Fade(PARCHMENT, alpha));
            DrawRectangle((int)x + 3, (int)y, 4, 1, Fade(PARCHMENT, alpha));
            break;
    }
}

// The journal's "walk from head" line is made of tokens such as N1, ->, NULL.
enum class TokenStyle { Plain, Unvisited, Current, Null, LoopEnd, Arrow };

struct Token {
    char text[24];
    TokenStyle style;
};

int addToken(Token* tokens, int count, int capacity, const char* text, TokenStyle style) {
    if (count >= capacity) return count;
    std::snprintf(tokens[count].text, sizeof(tokens[count].text), "%s", text);
    tokens[count].style = style;
    return count + 1;
}

// Lays tokens out left to right, wrapping at maxX. Draws them only if `draw` is true.
int layoutTokens(const Token* tokens, int count, float x, float y, float maxX, bool draw, Color ink) {
    float cursor = x;
    int lines = 1;
    for (int i = 0; i < count; i++) {
        int width = MeasureText(tokens[i].text, 20);
        int extra = (tokens[i].style == TokenStyle::Null) ? 16 : 0; // room for the stop mark
        if (cursor + width + extra > maxX && cursor > x) {
            cursor = x;
            y += 26.0f;
            lines++;
        }
        if (draw) {
            Color color = INK;
            if (tokens[i].style == TokenStyle::Arrow) color = INK_SOFT;
            if (tokens[i].style == TokenStyle::Unvisited) color = Fade(INK_SOFT, 0.75f); // not walked yet
            if (tokens[i].style == TokenStyle::LoopEnd) color = ink;
            if (tokens[i].style == TokenStyle::Null) color = INK_NULL;
            if (tokens[i].style == TokenStyle::Current) { // the node you are on: a gold box around it
                DrawRectangleRounded(Rectangle{cursor - 3.0f, y - 2.0f, width + 6.0f, 22.0f}, 0.3f, 4, Fade(CURRENT_GOLD, 0.55f));
            }
            DrawText(tokens[i].text, (int)cursor, (int)y, 20, color);
            if (tokens[i].style == TokenStyle::Null) drawStopMark(cursor + width + 9.0f, y + 9.0f, INK_NULL);
        }
        cursor += width + 8.0f + extra;
    }
    return lines;
}

// "N3 - Fallen Log" for a node, "NULL" for nullptr.
void describeNode(char* out, size_t size, const Node* node) {
    if (node == nullptr) {
        std::snprintf(out, size, "NULL");
    } else {
        std::snprintf(out, size, "N%d - %s", node->id, node->label);
    }
}

// Little ink sketches for the summary page: a leaf, an arch, a flower, a bridge.
void drawRegionSketch(int region, float x, float y, Color ink) {
    switch (region) {
        case 0:
            DrawEllipse((int)x, (int)y, 10.0f, 5.0f, ink);
            DrawLineEx(Vector2{x - 12.0f, y + 4.0f}, Vector2{x + 10.0f, y - 2.0f}, 1.5f, PARCHMENT);
            break;
        case 1:
            DrawRing(Vector2{x, y + 6.0f}, 7.0f, 11.0f, 180.0f, 360.0f, 16, ink);
            DrawRectangle((int)x - 11, (int)y + 6, 4, 8, ink);
            DrawRectangle((int)x + 7, (int)y + 6, 4, 8, ink);
            break;
        case 2:
            for (int i = 0; i < 5; i++) {
                float angle = i * 1.2566f;
                DrawCircleV(Vector2{x + std::cos(angle) * 6.0f, y + std::sin(angle) * 6.0f}, 4.0f, ink);
            }
            DrawCircleV(Vector2{x, y}, 3.0f, GILT);
            break;
        case 3:
            DrawRectangle((int)x - 14, (int)y, 28, 3, ink);
            DrawRectangle((int)x - 14, (int)y - 8, 2, 11, ink);
            DrawRectangle((int)x + 12, (int)y - 8, 2, 11, ink);
            for (int i = 0; i < 5; i++) DrawRectangle((int)x - 11 + i * 5, (int)y + 3, 3, 2, Fade(ink, 0.6f));
            break;
        case 4: // a stack of stones, the top one lighter
            for (int i = 0; i < 4; i++) {
                int w = 22 - i * 3;
                DrawRectangleRounded(Rectangle{x - w / 2.0f + ((i % 2) ? 1.0f : -1.0f), y + 8.0f - i * 6.0f, (float)w, 5.0f}, 0.5f, 4,
                                     (i == 3) ? Fade(ink, 0.55f) : ink);
            }
            break;
        case 5: // a little boat with a sail
            drawTriangleSafe(Vector2{x - 13.0f, y + 4.0f}, Vector2{x + 13.0f, y + 4.0f}, Vector2{x + 8.0f, y + 10.0f}, ink);
            DrawRectangle((int)x - 13, (int)y + 4, 21, 6, ink);
            DrawRectangle((int)x - 1, (int)y - 12, 2, 16, ink);
            drawTriangleSafe(Vector2{x + 2.0f, y - 11.0f}, Vector2{x + 2.0f, y + 1.0f}, Vector2{x + 11.0f, y + 1.0f}, Fade(ink, 0.6f));
            break;
        default:
            break;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Frame layout
// ---------------------------------------------------------------------------

// The world is drawn off screen first (its canvas, light map, and bloom are texture passes of
// their own); then the frame is composed in 1280x720 coordinates and shown in the window or
// fullscreen (see Display.h).
void Game::draw() const {
    const Mood mood = drawScene();
    display.beginFrame();
    renderer.present(mood, SCREEN_WIDTH, SCREEN_HEIGHT);
    drawInterface();
    display.endFrame();
}

void Game::drawInterface() const {
    // The interface for play sits under the fades, so it fades out and in with the scene
    // instead of switching to the next region's HUD while the screen is black.
    if (state != GameState::Complete) {
        drawHud();
        drawJournalHint();
        if (inspectorOpen) drawInspector();
        drawToast();
        drawCallout();
        drawInteractionPrompt();
        if (state == GameState::Playing) drawControlsStrip();
        if (state == GameState::Message) drawMessageBox();
    }

    if (fadeTimer > 0.0f) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, fadeTimer / RECOVERY_FADE_SECONDS));
    }
    if (transitionTarget >= 0) { // fade out, hold in the dark, then fade back in
        float alpha = transitionSwitched
                          ? 1.0f - (transitionTimer - TRANSITION_OUT_SECONDS - TRANSITION_HOLD_SECONDS) / TRANSITION_IN_SECONDS
                          : transitionTimer / TRANSITION_OUT_SECONDS;
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, std::fmin(1.0f, std::fmax(0.0f, alpha))));
    }
    if (state == GameState::Complete) {
        drawCompleteScreen();
        return;
    }

    drawTitleCard(); // the place's name, over the dark beat and the fade-in
    if (state == GameState::Paused) drawPauseScreen();
}

// ---------------------------------------------------------------------------
// While exploring
// ---------------------------------------------------------------------------

// Top left: where you are and what to do.
void Game::drawHud() const {
    const RegionInfo& region = world.getRegion(regionIndex);
    const RegionStyle& style = REGION_STYLES[regionIndex];

    char placeLine[96];
    std::snprintf(placeLine, sizeof(placeLine), "%s", TextToUpper(region.placeName));
    char kindLine[64];
    std::snprintf(kindLine, sizeof(kindLine), "%s", TextToUpper(region.title));
    const char* objective = currentObjective();

    int width = MeasureText(placeLine, 10) + 16 + MeasureText(kindLine, 10);
    int objectiveWidth = MeasureText(objective, 20);
    if (objectiveWidth > width) width = objectiveWidth;

    Rectangle card = {12.0f, 10.0f, width + 40.0f, 50.0f};
    DrawRectangleRounded(card, 0.25f, 6, Fade(UI_NAVY, 0.78f));
    DrawRectangleRounded(Rectangle{card.x + 10.0f, card.y + 10.0f, 3.0f, 30.0f}, 1.0f, 4, style.accent);
    DrawText(placeLine, (int)card.x + 22, (int)card.y + 9, 10, style.accent);
    DrawText(kindLine, (int)card.x + 22 + MeasureText(placeLine, 10) + 16, (int)card.y + 9, 10, UI_DIM_TEXT);
    DrawText(objective, (int)card.x + 22, (int)card.y + 22, 20, OBJECTIVE_YELLOW);
}

// Top right: where the journal and the pause menu are.
void Game::drawJournalHint() const {
    if (inspectorOpen) {
        return; // the journal itself is open there
    }
    const bool muted = audio.settings().muted;
    float width = 24.0f + measureKeyHint("TAB", "journal") + 16.0f + measureKeyHint("ESC", "pause");
    if (muted) width += 16.0f + measureKeyHint("M", "sound off");
    Rectangle card = {SCREEN_WIDTH - 12.0f - width, 10.0f, width, 26.0f};
    DrawRectangleRounded(card, 0.4f, 6, Fade(UI_NAVY, 0.7f));
    float x = card.x + 12.0f;
    x += drawKeyHint("TAB", "journal", x, card.y + 6.0f, UI_TEXT, OUTLINE_DARK, UI_DIM_TEXT) + 16.0f;
    x += drawKeyHint("ESC", "pause", x, card.y + 6.0f, UI_TEXT, OUTLINE_DARK, UI_DIM_TEXT) + 16.0f;
    if (muted) drawKeyHint("M", "sound off", x, card.y + 6.0f, NULL_AMBER, OUTLINE_DARK, NULL_AMBER);
}

// A short line about what just happened, under the objective (exploring voice).
void Game::drawToast() const {
    if (toastTimer <= 0.0f || toastText.empty()) {
        return;
    }
    float alpha = toastTimer < 0.5f ? toastTimer / 0.5f : 1.0f;
    int width = MeasureText(toastText.c_str(), 20);
    float y = 66.0f;
    DrawRectangleRounded(Rectangle{12.0f, y, width + 34.0f, 30.0f}, 0.3f, 4, Fade(UI_NAVY, 0.78f * alpha));
    DrawCircleV(Vector2{25.0f, y + 15.0f}, 3.0f, Fade(REGION_STYLES[regionIndex].accent, alpha));
    DrawText(toastText.c_str(), 36, (int)y + 5, 20, Fade(UI_TEXT, alpha));
}

// The first time a kind of link is used: a slip from the journal naming it, with one code line.
void Game::drawCallout() const {
    if (calloutTimer <= 0.0f || inspectorOpen) {
        return;
    }
    const CalloutText& text = CALLOUTS[(int)calloutShown];
    const RegionStyle& style = REGION_STYLES[regionIndex];
    float shown = CALLOUT_SECONDS - calloutTimer;
    float alpha = std::fmin(1.0f, shown / 0.35f) * std::fmin(1.0f, calloutTimer / 0.6f);
    float slide = (1.0f - smooth01(shown / 0.35f)) * 14.0f;

    char title[64];
    std::snprintf(title, sizeof(title), "%s", TextToUpper(text.title));
    float content = (float)MeasureText(title, 10) + 24.0f + measureKeyHint("TAB", "more");
    if (MeasureText(text.code, 20) > content) content = (float)MeasureText(text.code, 20);
    if (MeasureText(text.line, 10) > content) content = (float)MeasureText(text.line, 10);
    Rectangle card = {SCREEN_WIDTH - 12.0f - (content + 76.0f), 44.0f - slide, content + 76.0f, 80.0f};

    drawParchment(card, alpha);
    drawNoteMark(NoteKind::Info, card.x + 30.0f, card.y + 40.0f, style.ink, alpha);
    float x = card.x + 56.0f;
    DrawText(title, (int)x, (int)card.y + 13, 10, Fade(style.ink, alpha));
    drawKeyHint("TAB", "more", card.x + card.width - 20.0f - measureKeyHint("TAB", "more"), card.y + 11.0f, INK, PARCHMENT,
                INK_SOFT, alpha);
    DrawText(text.code, (int)x, (int)card.y + 30, 20, Fade(CODE_INK, alpha));
    DrawText(text.line, (int)x, (int)card.y + 57, 10, Fade(INK, alpha));
}

// A key hint above the traveler where the place has something to do: Region 4's NULL link on N3,
// Region 5's stair (push, the bell, pop), and Region 6's rear boat (enqueue).
void Game::drawInteractionPrompt() const {
    const Node* current = player.currentNode;
    if (state != GameState::Playing || current == nullptr || transitionTarget >= 0) {
        return;
    }
    const char* key = nullptr;
    const char* action = nullptr;
    Color color = REGION_STYLES[regionIndex].accent;
    if (regionIndex == 3 && stage == 1 && current->id == 3) {
        key = inspectorOpen ? "E" : "TAB";
        action = inspectorOpen ? "mend the bridge" : "look at the way ahead";
        color = NULL_AMBER;
    } else if (regionIndex == 4 && !gateOpen && player.grounded && current == world.getHead(regionIndex)) {
        if (stage == 0) {
            key = "E";
            action = (current->id == World::STONE_SLOTS) ? "ring the bell" : "set a stone on top";
        } else if (current->next != nullptr) {
            key = "S";
            action = "take the top stone off";
        }
    } else if (regionIndex == 5 && !gateOpen && player.grounded && current->next == nullptr && world.boatsRemain()) {
        key = "E";
        action = "call a boat";
    }
    if (key == nullptr) {
        return;
    }
    float width = measureKeyHint(key, action) + 14.0f;
    float x = player.position.x + player.size.x / 2.0f - camera.target.x - width / 2.0f;
    float y = player.position.y - 34.0f;
    if (regionIndex == 4) { // on the stair: beside the traveler, so the bell above stays in view
        x = player.position.x + player.size.x + 14.0f - camera.target.x;
        y = player.position.y + 6.0f;
    }
    DrawRectangleRounded(Rectangle{x, y, width, 22.0f}, 0.4f, 4, Fade(UI_NAVY, 0.9f));
    DrawRectangleRoundedLinesEx(Rectangle{x, y, width, 22.0f}, 0.4f, 4, 1.5f, color);
    drawKeyHint(key, action, x + 7.0f, y + 4.0f, UI_TEXT, OUTLINE_DARK, color);
}

// Bottom left, small and quiet: the controls at the start of the journey. They fade out once
// the traveler has made the first step (the pause screen lists them too).
void Game::drawControlsStrip() const {
    float alpha = journeyStarted ? 1.0f - journeyTimer / 1.5f : 1.0f;
    if (alpha <= 0.0f) {
        return;
    }
    struct Hint { const char* key; const char* action; };
    const Hint hints[] = {
        {"A/D or ARROWS", "walk"}, {"SPACE", "jump"}, {"E/ENTER", "interact"}, {"TAB", "journal"}, {"ESC", "pause"},
    };
    float total = 0.0f;
    for (const Hint& hint : hints) total += measureKeyHint(hint.key, hint.action) + 14.0f;
    total -= 14.0f;

    float x = 22.0f;
    float y = SCREEN_HEIGHT - 26.0f;
    DrawRectangleRounded(Rectangle{x - 10.0f, y - 4.0f, total + 20.0f, 22.0f}, 0.5f, 6, Fade(UI_NAVY, 0.6f * alpha));
    for (const Hint& hint : hints) {
        x += drawKeyHint(hint.key, hint.action, x, y, UI_TEXT, OUTLINE_DARK, UI_DIM_TEXT, alpha) + 14.0f;
    }
}

// Between places: the screen holds dark while the place's name and a line about it appear among
// drifting motes of its light, then the words rise and fade as the place itself fades in.
void Game::drawTitleCard() const {
    if (titleCardTimer <= 0.0f) {
        return;
    }
    const RegionInfo& region = world.getRegion(regionIndex);
    const RegionStyle& style = REGION_STYLES[regionIndex];
    float shown = TITLE_CARD_SECONDS - titleCardTimer;
    float alpha = std::fmin(1.0f, shown / 0.6f) * std::fmin(1.0f, titleCardTimer / 0.8f);
    float dark = 1.0f - smooth01((shown - TRANSITION_HOLD_SECONDS) / TRANSITION_IN_SECONDS); // 1 while held dark

    // A faint glow of the place's color behind the words, so the dark has a hint of where you are going.
    DrawCircleGradient(SCREEN_WIDTH / 2, 330, 520.0f, Fade(style.accent, 0.07f * alpha * dark), Fade(style.accent, 0.0f));
    for (int i = 0; i < 40; i++) { // motes of the place's light, rising through the dark
        float speed = 0.05f + 0.07f * hash01(i * 3 + 1);
        float t = std::fmod(shown * speed + hash01(i * 7 + 2), 1.0f);
        float x = SCREEN_WIDTH * hash01(i * 11 + 3) + std::sin(shown * 0.8f + i * 1.3f) * 16.0f;
        float y = SCREEN_HEIGHT * (1.05f - t * 1.1f);
        float a = alpha * dark * std::sin(t * 3.14159f) * (0.35f + 0.55f * hash01(i * 5));
        float r = 1.0f + 1.5f * hash01(i * 13);
        DrawCircleV(Vector2{x, y}, r * 4.0f, Fade(style.accent, a * 0.12f));
        DrawCircleV(Vector2{x, y}, r, Fade(style.accent, a));
    }

    float rise = 1.0f - dark;
    const float center = SCREEN_WIDTH / 2.0f;
    const int y = (int)(286.0f - 124.0f * rise);
    const int nameWidth = MeasureText(region.placeName, 40);
    if (rise > 0.0f) { // a soft band behind the words once the scene shows through
        float band = 0.45f * alpha * rise;
        DrawRectangleGradientH((int)center - nameWidth, y - 18, nameWidth, 140, Fade(BLACK, 0.0f), Fade(BLACK, band));
        DrawRectangleGradientH((int)center, y - 18, nameWidth, 140, Fade(BLACK, band), Fade(BLACK, 0.0f));
    }
    char number[32];
    std::snprintf(number, sizeof(number), "REGION %d", regionIndex + 1);
    DrawText(number, (int)center - MeasureText(number, 20) / 2, y, 20, Fade(GILT, alpha));
    DrawText(region.placeName, (int)center - nameWidth / 2 + 2, y + 30, 40, Fade(BLACK, 0.6f * alpha));
    DrawText(region.placeName, (int)center - nameWidth / 2, y + 28, 40, Fade(PARCHMENT, alpha));

    float half = 110.0f * smooth01(shown / 1.0f); // a thin rule drawn out from a diamond
    DrawRectangle((int)(center - half), y + 84, (int)(half * 2.0f), 1, Fade(style.accent, 0.7f * alpha));
    DrawPoly(Vector2{center, y + 84.5f}, 4, 4.0f, 0.0f, Fade(style.accent, alpha));

    const char* line = REGION_LINES[regionIndex];
    float lineAlpha = alpha * smooth01((shown - 0.35f) / 0.6f);
    DrawText(line, (int)center - MeasureText(line, 20) / 2, y + 98, 20, Fade(style.accent, lineAlpha));
}

// ---------------------------------------------------------------------------
// The field journal: the current node's real links, the walk from the head, and the lesson.
// ---------------------------------------------------------------------------

void Game::drawInspector() const {
    const RegionInfo& region = world.getRegion(regionIndex);
    const RegionStyle& style = REGION_STYLES[regionIndex];
    Node* head = world.getHead(regionIndex);
    const Node* current = player.currentNode;
    bool isDoubly = region.kind == ListKind::Doubly;
    bool isCircular = region.kind == ListKind::Circular;
    bool isStack = region.kind == ListKind::Stack;
    bool isQueue = region.kind == ListKind::Queue;
    bool emphasizePrevious = regionIndex == 1 && stage == 1 && !gateOpen;

    // Build the "walk from head" line by reading the actual links.
    const int capacity = 40;
    Token tokens[capacity];
    int count = 0;
    const char* stopNote = "stopped: safety step limit reached";
    if (isDoubly && head != nullptr && head->previous == nullptr) {
        count = addToken(tokens, count, capacity, "NULL", TokenStyle::Null);
        count = addToken(tokens, count, capacity, "<-", TokenStyle::Arrow);
    }
    Node* node = head;
    int steps = 0;
    char id[16];
    while (node != nullptr && steps < MAX_NODES_PER_REGION) {
        std::snprintf(id, sizeof(id), "N%d", node->id);
        bool visited = node->id >= 0 && node->id <= MAX_NODES_PER_REGION && visitedIds[node->id];
        TokenStyle look = (node == current) ? TokenStyle::Current : (visited ? TokenStyle::Plain : TokenStyle::Unvisited);
        count = addToken(tokens, count, capacity, id, look);
        Node* next = node->next;
        steps++;
        if (next == nullptr) {
            count = addToken(tokens, count, capacity, "->", TokenStyle::Arrow);
            count = addToken(tokens, count, capacity, "NULL", TokenStyle::Null);
            stopNote = isStack ? "stopped: next == NULL (the bottom of the stack)"
                               : (isQueue ? "stopped: next == NULL (the rear of the queue)" : "stopped: next == NULL (the end of the list)");
            break;
        }
        if (next == head) { // circular list: the stop condition fires
            std::snprintf(id, sizeof(id), "N%d", head->id);
            count = addToken(tokens, count, capacity, "->", TokenStyle::Arrow);
            count = addToken(tokens, count, capacity, id, TokenStyle::LoopEnd);
            stopNote = "stopped: back at the start (node == head)";
            break;
        }
        bool twoWay = isDoubly && next->previous == node;
        count = addToken(tokens, count, capacity, twoWay ? "<->" : "->", TokenStyle::Arrow);
        node = next;
    }

    // A line that connects the list to where the traveler is and what they are doing.
    const char* note = nullptr;
    if (current != nullptr) {
        if (regionIndex == 3 && stage == 1 && current->next == nullptr) {
            note = "next is NULL: there is no node to follow. Press E to append one.";
        } else if (regionIndex == 3 && stage >= 2 && current->id == 3) {
            note = "next is N4 (Bridge) now: append() linked it to the tail.";
        } else if (emphasizePrevious && current->previous != nullptr) {
            note = "The key's light goes back through previous, to N2 (Old Hall).";
        } else if (isDoubly && current->previous == nullptr) {
            note = "previous is NULL: this is the head.";
        } else if (current->next == nullptr) {
            note = "next is NULL: this is the tail.";
        } else if (isCircular && current->next == head) {
            note = "next is the head: this is the tail of the loop.";
        }
        if (isStack) { // the stair: what push and pop would do from here
            if (head != current) note = "push() set a new stone on top. Climb onto it.";
            else if (gateOpen || (stage >= 1 && current->next == nullptr)) note = "N1's next is NULL: it is the bottom of the stack.";
            else if (stage == 0 && current->next == nullptr) note = "N1 is the whole stack. E pushes a stone onto it.";
            else if (stage == 0 && current->id < World::STONE_SLOTS) note = "E pushes a stone. Its next will be this stone.";
            else if (stage == 0) note = "The stair reaches the bell. Press E to ring it.";
            else note = "S pops: top = top->next, then the old top is deleted.";
        } else if (isQueue) { // the line of boats: the two ends
            if (current->next == nullptr) {
                note = world.boatsRemain() ? "This is the rear. E: rear->next = boat; rear = boat;" : "This is the rear, and no more boats are coming.";
            } else if (current == head) {
                note = "You stand on the front. Step off and it leaves: dequeue().";
            }
        }
    }
    char stepsLine[96] = "";
    if (isCircular) {
        std::snprintf(stepsLine, sizeof(stepsLine), "Steps this walk: %d.  Stop when current == N%d.  Safety limit: %d.",
                      loopSteps, (loopStartNode != nullptr) ? loopStartNode->id : 1, MAX_LOOP_STEPS);
    } else if (isStack && head != nullptr) {
        std::snprintf(stepsLine, sizeof(stepsLine), "Stones in the stack: %d.  The top is N%d.", steps, head->id);
    } else if (isQueue && head != nullptr) {
        const Node* rear = world.getQueueRear();
        std::snprintf(stepsLine, sizeof(stepsLine), "Boats in the queue: %d.  Front N%d, rear N%d.", steps, head->id,
                      (rear != nullptr) ? rear->id : 0);
    }
    const bool hasStepsLine = stepsLine[0] != '\0';
    const char* lesson = REGION_LESSONS[regionIndex];

    const float width = 412.0f;
    const float x = SCREEN_WIDTH - width - 10.0f;
    const float pad = 18.0f;
    const float valueX = x + pad + 134.0f;
    const float rightX = x + width - pad;
    int walkLines = layoutTokens(tokens, count, x + pad, 0.0f, rightX, false, style.ink);
    float height = 40.0f + 24.0f + 16.0f + 26.0f + 26.0f + 30.0f + (isQueue ? 26.0f : 0.0f) + 12.0f + 16.0f +
                   walkLines * 26.0f + 16.0f + (note != nullptr ? 16.0f : 0.0f) + (hasStepsLine ? 16.0f : 0.0f) + 10.0f +
                   16.0f + countLines(lesson) * 14.0f + 12.0f;
    float y = 10.0f;

    drawParchment(Rectangle{x, y, width, height});
    DrawRectangleRounded(Rectangle{x + 12.0f, y + 8.0f, 150.0f, 22.0f}, 0.3f, 4, LEATHER); // the journal's tab
    DrawText("FIELD JOURNAL", (int)x + 22, (int)y + 14, 10, GILT);
    DrawRectangle((int)(x + width - 40.0f), (int)y, 10, 30, style.ink); // a ribbon bookmark in the region's color
    drawTriangleSafe(Vector2{x + width - 40.0f, y + 30.0f}, Vector2{x + width - 30.0f, y + 30.0f},
                     Vector2{x + width - 35.0f, y + 25.0f}, PARCHMENT);
    drawKeyHint("TAB", "close", x + width - 52.0f - measureKeyHint("TAB", "close"), y + 12.0f, INK, PARCHMENT, INK_SOFT);
    y += 40.0f;

    DrawText(region.title, (int)(x + pad), (int)y, 20, style.ink);
    y += 24.0f;

    char text[48];
    if (current == nullptr) {
        DrawText("current = NULL (no node!)", (int)(x + pad), (int)y, 20, INK_NULL);
        y += 98.0f;
    } else {
        DrawText("YOU ARE ON", (int)(x + pad), (int)y, 10, INK_SOFT);
        y += 16.0f;
        describeNode(text, sizeof(text), current);
        DrawPoly(Vector2{x + pad + 6.0f, y + 9.0f}, 4, 7.0f, 0.0f, CURRENT_GOLD);
        DrawText(text, (int)(x + pad + 18.0f), (int)y, 20, INK);
        y += 26.0f;

        // next: every list kind stores it.
        describeNode(text, sizeof(text), current->next);
        DrawText("next", (int)(x + pad), (int)y, 20, style.ink);
        DrawText("->", (int)(valueX - 30.0f), (int)y, 20, INK_SOFT);
        DrawText(text, (int)valueX, (int)y, 20, current->next != nullptr ? INK : INK_NULL);
        if (current->next == nullptr) drawStopMark(valueX + MeasureText(text, 20) + 10.0f, y + 9.0f, INK_NULL);
        else if (isStack) DrawText("under it", (int)(rightX - MeasureText("under it", 10)), (int)y + 6, 10, INK_SOFT);
        else if (!emphasizePrevious) DrawText("follow", (int)(rightX - MeasureText("follow", 10)), (int)y + 6, 10, style.ink);
        y += 26.0f;

        // The stack and the queue keep pointers to their ends; the doubly linked list stores previous.
        auto drawEnd = [&](const char* name, const Node* end, const char* tag) {
            describeNode(text, sizeof(text), end);
            DrawText(name, (int)(x + pad), (int)y, 20, style.ink);
            DrawText("->", (int)(valueX - 30.0f), (int)y, 20, INK_SOFT);
            DrawText(text, (int)valueX, (int)y, 20, end != nullptr ? INK : INK_NULL);
            if (end == nullptr) drawStopMark(valueX + MeasureText(text, 20) + 10.0f, y + 9.0f, INK_NULL);
            else if (tag != nullptr) DrawText(tag, (int)(rightX - MeasureText(tag, 10)), (int)y + 6, 10, style.ink);
        };
        if (isStack) {
            drawEnd("top", head, (head == current) ? "you are here" : "climb");
        } else if (isQueue) {
            drawEnd("front", head, (head == current) ? "you are here" : nullptr);
            y += 26.0f;
            drawEnd("rear", world.getQueueRear(), world.boatsRemain() ? "E adds here" : nullptr);
        } else if (isDoubly) {
            describeNode(text, sizeof(text), current->previous);
            DrawText("previous", (int)(x + pad), (int)y, 20, style.ink);
            DrawText("->", (int)(valueX - 30.0f), (int)y, 20, INK_SOFT);
            DrawText(text, (int)valueX, (int)y, 20, current->previous != nullptr ? INK : INK_NULL);
            if (current->previous == nullptr) drawStopMark(valueX + MeasureText(text, 20) + 10.0f, y + 9.0f, INK_NULL);
            else if (emphasizePrevious) DrawText("follow", (int)(rightX - MeasureText("follow", 10)), (int)y + 6, 10, style.ink);
        } else {
            DrawText("previous", (int)(x + pad), (int)y, 20, Fade(INK_SOFT, 0.6f));
            DrawText("not stored in this list", (int)valueX, (int)(y + 6.0f), 10, INK_SOFT);
        }
        y += 30.0f;
    }

    DrawRectangle((int)(x + pad), (int)y, (int)(width - 2.0f * pad), 1, Fade(INK_SOFT, 0.5f));
    y += 12.0f;
    DrawText(isStack ? "WALK FROM THE TOP" : (isQueue ? "WALK FROM THE FRONT" : "WALK FROM HEAD"), (int)(x + pad), (int)y, 10, INK_SOFT);
    y += 16.0f;
    layoutTokens(tokens, count, x + pad, y, rightX, true, style.ink);
    y += walkLines * 26.0f;
    DrawText(stopNote, (int)(x + pad), (int)y, 10, INK_SOFT);
    y += 16.0f;
    if (hasStepsLine) {
        DrawText(stepsLine, (int)(x + pad), (int)y, 10, INK);
        y += 16.0f;
    }
    if (note != nullptr) {
        DrawText(note, (int)(x + pad), (int)y, 10, (regionIndex == 3 && stage == 1) ? INK_NULL : INK);
        y += 16.0f;
    }

    y += 4.0f;
    DrawRectangle((int)(x + pad), (int)y, (int)(width - 2.0f * pad), 1, Fade(INK_SOFT, 0.5f));
    y += 6.0f;
    DrawText("WHAT THIS PLACE TEACHES", (int)(x + pad), (int)y, 10, INK_SOFT);
    y += 16.0f;
    drawLines(lesson, x + pad, y, 10, 14.0f, INK);
}

// ---------------------------------------------------------------------------
// Journal notes, pause, and the ending
// ---------------------------------------------------------------------------

// A note pinned near the bottom of the screen. It closes after its last page.
void Game::drawMessageBox() const {
    if (pageIndex >= pages.size()) return;
    const std::string& text = pages[pageIndex];
    NoteKind kind = (pageIndex < pageKinds.size()) ? pageKinds[pageIndex] : NoteKind::Info;
    const RegionStyle& style = REGION_STYLES[regionIndex];

    int lines = countLines(text.c_str());
    float width = MeasureText(text.c_str(), 20) + 96.0f;
    if (width < 460.0f) width = 460.0f;
    if (width > SCREEN_WIDTH - 40.0f) width = SCREEN_WIDTH - 40.0f;
    float height = lines * 22.0f + 52.0f;
    Rectangle card = {SCREEN_WIDTH / 2.0f - width / 2.0f, SCREEN_HEIGHT - height - 14.0f, width, height};

    drawParchment(card);
    DrawRectanglePro(Rectangle{card.x + 22.0f, card.y - 4.0f, 40.0f, 12.0f}, Vector2{0.0f, 0.0f}, -6.0f,
                     Fade(PARCHMENT_SHADE, 0.85f)); // a strip of tape
    drawNoteMark(kind, card.x + 36.0f, card.y + 30.0f, style.ink);
    DrawText(text.c_str(), (int)card.x + 64, (int)card.y + 16, 20, INK);

    float footerY = card.y + card.height - 22.0f;
    for (size_t i = 0; i < pages.size(); i++) { // page marks: filled = this page
        Vector2 dot = {card.x + 68.0f + i * 12.0f, footerY + 7.0f};
        if (i == pageIndex) DrawCircleV(dot, 3.5f, style.ink);
        else DrawRing(dot, 2.0f, 3.5f, 0.0f, 360.0f, 12, INK_SOFT);
    }
    const char* action = (pageIndex + 1 < pages.size()) ? "next" : "close";
    float hintWidth = measureKeyHint("E / ENTER", action);
    drawKeyHint("E / ENTER", action, card.x + card.width - hintWidth - 20.0f, footerY, INK, PARCHMENT, INK_SOFT);
}

// The journal's leather cover, closed while the game is paused: the controls on the left, and the
// sound on the right (up and down choose a row, left and right change it).
void Game::drawPauseScreen() const {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.55f));
    Rectangle cover = {SCREEN_WIDTH / 2.0f - 400.0f, 135.0f, 800.0f, 450.0f};
    DrawRectangleRounded(Rectangle{cover.x + 5.0f, cover.y + 6.0f, cover.width, cover.height}, 0.05f, 6, Fade(BLACK, 0.4f));
    DrawRectangleRounded(cover, 0.05f, 6, LEATHER);
    DrawRectangleRoundedLinesEx(Rectangle{cover.x + 10.0f, cover.y + 10.0f, cover.width - 20.0f, cover.height - 20.0f}, 0.05f,
                                6, 2.0f, GILT);
    DrawRectangle((int)cover.x + 28, (int)cover.y, 12, (int)cover.height, LEATHER_DARK); // the spine strap

    const float center = cover.x + cover.width / 2.0f + 14.0f; // a little right of the strap
    const char* title = "PAUSED";
    DrawText(title, (int)center - MeasureText(title, 40) / 2, (int)cover.y + 32, 40, GILT);
    const char* subtitle = TextFormat("Region %d - %s", regionIndex + 1, world.getRegion(regionIndex).placeName);
    DrawText(subtitle, (int)center - MeasureText(subtitle, 20) / 2, (int)cover.y + 80, 20, PARCHMENT);
    DrawRectangle((int)center - 330, (int)cover.y + 114, 660, 1, Fade(GILT, 0.6f));

    // Left page: the controls.
    const float leftX = cover.x + 70.0f;
    DrawText("CONTROLS", (int)leftX, (int)cover.y + 132, 10, GILT);
    struct Row { const char* key; const char* action; };
    const Row rows[] = {
        {"A / D  or  LEFT / RIGHT", "walk"},
        {"SPACE", "jump"},
        {"E  or  ENTER", "interact, turn a note's page"},
        {"S  or  DOWN", "take the top stone off (Region 5)"},
        {"TAB", "open or close the field journal"},
        {"ESC", "pause and resume"},
        {"M", "sound off or on"},
        {"F11  or  ALT+ENTER", "fullscreen or window"},
    };
    float rowY = cover.y + 152.0f;
    const float keysRight = leftX + 150.0f;
    for (const Row& row : rows) { // key caps right-aligned, actions beside them
        float capWidth = MeasureText(row.key, 10) + 12.0f;
        float capX = keysRight - capWidth;
        DrawRectangleRounded(Rectangle{capX, rowY, capWidth, 16.0f}, 0.3f, 4, PARCHMENT);
        DrawText(row.key, (int)(capX + 6.0f), (int)rowY + 3, 10, LEATHER_DARK);
        DrawText(row.action, (int)(keysRight + 12.0f), (int)rowY + 3, 10, PARCHMENT);
        rowY += 26.0f;
    }

    // Right page: the sound, and the screen.
    const float rightX = cover.x + 470.0f;
    DrawRectangle((int)rightX - 34, (int)cover.y + 132, 1, 236, Fade(GILT, 0.35f));
    DrawText("SOUND", (int)rightX, (int)cover.y + 132, 10, GILT);
    DrawText("SCREEN", (int)rightX, (int)cover.y + 270, 10, GILT);
    const AudioSettings& s = audio.settings();
    const char* labels[4] = {"Music", "Sounds", "Mute", "Mode"};
    const float rowsY[4] = {cover.y + 156.0f, cover.y + 192.0f, cover.y + 228.0f, cover.y + 292.0f};
    const float values[2] = {s.music, s.sounds};
    for (int i = 0; i < 4; i++) {
        float y = rowsY[i];
        bool selected = settingsRow == i;
        if (selected) { // a gilt marker and a soft band on the chosen row
            DrawRectangleRounded(Rectangle{rightX - 14.0f, y - 7.0f, 300.0f, 30.0f}, 0.4f, 6, Fade(GILT, 0.14f));
            DrawPoly(Vector2{rightX - 6.0f, y + 8.0f}, 4, 5.0f, 0.0f, GILT);
        }
        Color labelColor = selected ? GILT : PARCHMENT;
        DrawText(labels[i], (int)rightX + 6, (int)y, 20, labelColor);
        if (i < 2) {
            const float barX = rightX + 96.0f;
            const int notches = 10;
            int filled = (int)std::round(values[i] * notches);
            for (int n = 0; n < notches; n++) { // ten notches, filled up to the level
                Rectangle notch = {barX + n * 15.0f, y + 3.0f, 11.0f, 14.0f};
                if (n < filled) {
                    DrawRectangleRec(notch, Fade(s.muted ? PARCHMENT_SHADE : PARCHMENT, s.muted ? 0.5f : 1.0f));
                } else {
                    DrawRectangleLinesEx(notch, 1.0f, Fade(PARCHMENT, 0.45f));
                }
            }
            const char* percent = TextFormat("%d%%", (int)std::round(values[i] * 100.0f));
            Color percentColor = (selected && audioRowFlash > 0.0f) ? GILT : PARCHMENT;
            DrawText(percent, (int)(barX + notches * 15.0f + 10.0f), (int)y, 20, percentColor);
        } else if (i == 2) {
            const char* muteText = s.muted ? "on: no sound" : "off";
            DrawText(muteText, (int)rightX + 96, (int)y, 20, s.muted ? NULL_AMBER : PARCHMENT);
        } else { // the screen: the window, or fullscreen at the monitor's own resolution
            const char* mode = display.isFullscreen() ? "fullscreen" : "window";
            DrawText(mode, (int)rightX + 96, (int)y, 20, PARCHMENT);
            const char* size = TextFormat("%d x %d", display.width(), display.height());
            DrawText(size, (int)rightX + 108 + MeasureText(mode, 20), (int)y + 6, 10, Fade(PARCHMENT, 0.7f));
        }
    }
    DrawText("W / S  or  UP / DOWN:  choose a row", (int)rightX - 6, (int)cover.y + 334, 10, Fade(PARCHMENT, 0.7f));
    DrawText("A / D  or  LEFT / RIGHT:  change it", (int)rightX - 6, (int)cover.y + 350, 10, Fade(PARCHMENT, 0.7f));
    if (!audio.isReady()) { // no audio device, or no assets/audio folder: say so instead of failing quietly
        DrawText("Sound is unavailable on this computer.", (int)rightX - 6, (int)cover.y + 368, 10, NULL_AMBER);
    }

    float total = measureKeyHint("ESC", "resume") + 30.0f + measureKeyHint("Q", "quit");
    float x = center - total / 2.0f;
    x += drawKeyHint("ESC", "resume", x, cover.y + cover.height - 50.0f, GILT, LEATHER_DARK, GILT) + 30.0f;
    drawKeyHint("Q", "quit", x, cover.y + cover.height - 50.0f, GILT, LEATHER_DARK, GILT);
}
// The journal opened to its last spread: what each place taught.
void Game::drawCompleteScreen() const {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.6f));
    Rectangle book = {120.0f, 70.0f, 1040.0f, 580.0f};
    DrawRectangleRounded(Rectangle{book.x - 14.0f, book.y - 14.0f, book.width + 28.0f, book.height + 28.0f}, 0.04f, 6, LEATHER);
    Rectangle left = {book.x, book.y, book.width / 2.0f - 6.0f, book.height};
    Rectangle right = {book.x + book.width / 2.0f + 6.0f, book.y, book.width / 2.0f - 6.0f, book.height};
    drawParchment(left);
    drawParchment(right);
    DrawRectangle((int)(book.x + book.width / 2.0f - 4.0f), (int)book.y, 8, (int)book.height, LEATHER_DARK); // the spine

    const float middle = left.x + left.width / 2.0f;
    const char* title = "The Wanderer's Journal";
    DrawText(title, (int)middle - MeasureText(title, 30) / 2, (int)left.y + 40, 30, INK);
    const char* subtitle = "The journey's end";
    DrawText(subtitle, (int)middle - MeasureText(subtitle, 20) / 2, (int)left.y + 82, 20, INK_SOFT);

    // A portrait of the traveler, drawn with the same sprite as in the game, four times larger.
    Player portrait = player;
    portrait.grounded = true;
    portrait.velocity = Vector2{0.0f, 0.0f};
    portrait.landTimer = 0.0f;
    portrait.facing = 1.0f;
    Vector2 portraitFeet = {middle, left.y + 320.0f};
    DrawEllipse((int)portraitFeet.x, (int)portraitFeet.y + 6, 70.0f, 10.0f, Fade(PARCHMENT_SHADE, 0.9f));
    drawTraveler(portrait, elapsed, portraitFeet, 4.0f);

    const char* journey = "Six places, six lists, one journey.";
    DrawText(journey, (int)middle - MeasureText(journey, 20) / 2, (int)left.y + 352, 20, INK);
    const char* closing = "Every link followed, and every gap mended.";
    DrawText(closing, (int)middle - MeasureText(closing, 10) / 2, (int)left.y + 386, 10, INK_SOFT);
    const char* music = "Music by Scott Buckley (scottbuckley.com.au), CC BY 4.0.";
    const char* sounds = "Sounds: Freesound, Kenney, and Versilian Studios, CC0. See assets/audio/CREDITS.md.";
    DrawText(music, (int)middle - MeasureText(music, 10) / 2, (int)left.y + 456, 10, INK_SOFT);
    DrawText(sounds, (int)middle - MeasureText(sounds, 10) / 2, (int)left.y + 472, 10, INK_SOFT);

    struct Entry { const char* line1; const char* line2; };
    const Entry entries[REGION_COUNT] = {
        {"Follow next, one way only.", "The tail's next is NULL."},
        {"next and previous go both ways.", "Both ends point to NULL."},
        {"The tail's next is the head.", "Stop when you are back at the start."},
        {"Check a pointer before you follow it.", "nullptr means there is no node."},
        {"Push and pop at one end, the top.", "The last in is the first out."},
        {"Add at the rear, take from the front.", "The first in is the first out."},
    };
    for (int i = 0; i < REGION_COUNT; i++) {
        const RegionInfo& region = world.getRegion(i);
        const RegionStyle& style = REGION_STYLES[i];
        float y = right.y + 22.0f + i * 92.0f;
        float x = right.x + 34.0f;
        drawRegionSketch(i, x + 14.0f, y + 14.0f, style.ink);
        DrawText(TextFormat("%d  %s", i + 1, region.placeName), (int)x + 44, (int)y, 20, style.ink);
        DrawText(region.title, (int)x + 44, (int)y + 23, 10, INK_SOFT);
        DrawText(entries[i].line1, (int)x + 44, (int)y + 38, 20, INK);
        DrawText(entries[i].line2, (int)x + 44, (int)y + 60, 20, INK);
    }

    float total = measureKeyHint("R", "walk again") + 40.0f + measureKeyHint("Q / ESC", "exit");
    float x = middle - total / 2.0f;
    float y = left.y + left.height - 60.0f;
    x += drawKeyHint("R", "walk again", x, y, INK, PARCHMENT, INK) + 40.0f;
    drawKeyHint("Q / ESC", "exit", x, y, INK, PARCHMENT, INK);
}

// ---------------------------------------------------------------------------
// Text state
// ---------------------------------------------------------------------------

void Game::showMessage(std::vector<std::string> newPages, NoteKind kind) {
    pages = std::move(newPages);
    pageKinds.assign(pages.size(), kind);
    pageIndex = 0;
    state = pages.empty() ? GameState::Playing : GameState::Message;
    if (!pages.empty()) {
        stopPlayerPose();
        // A welcome note is pinned up with a paper sound once it can be seen. Other notes arrive
        // with their own sound (a recovery, an exit opening), so they add none.
        notePending = kind == NoteKind::Intro;
    }
}

void Game::showToast(const std::string& text, float seconds) {
    toastText = text;
    toastTimer = seconds;
}

void Game::showCallout(Callout which) {
    int index = (int)which;
    if (index < 0 || index >= (int)Callout::Count || calloutSeen[index]) {
        return; // each kind of link is named once per journey
    }
    calloutSeen[index] = true;
    calloutShown = which;
    calloutTimer = CALLOUT_SECONDS;
    audio.play(Sfx::Callout, 1.0f, 1.0f, 0.6f, 0.4f); // a slip of paper, from the journal's side of the screen
}

// What to do next, in the exploring voice (the journal has the details).
const char* Game::currentObjective() const {
    switch (regionIndex) {
        case 0:
            return gateOpen ? "The arch is open. Walk through it." : "Follow the warm light to the sunlit arch.";
        case 1:
            if (gateOpen) return "The doorway is open. Walk through it.";
            if (stage == 0) return "Climb to the Key Shrine and take the key.";
            if (stage == 1) return "Carry the key's light back down to the Old Hall.";
            return "Go on to the Far Steps.";
        case 2:
            return gateOpen ? "You are home. Walk into the moon gate." : "Walk the whole circle and return to the gate.";
        case 3:
            if (gateOpen) return "Dawn has come. Walk through the gate.";
            if (stage == 0) return "Make your way to the cliff's edge.";
            if (stage == 1) return inspectorOpen ? "Mend the missing link." : "Look before you step: check the way ahead.";
            return "Cross the bridge to the far cliff.";
        case 4:
            if (gateOpen) return "The sea door is open. Walk through it.";
            if (stage == 0) return "Set stones and climb to the fog bell.";
            return "Take the stair down, top stone first.";
        case 5:
            if (gateOpen) return "The cabin door is open. Walk through it.";
            return world.boatsRemain() ? "Call boats and cross the harbor to the ship." : "The ship is in. Cross the boats to reach it.";
        default:
            return "";
    }
}
