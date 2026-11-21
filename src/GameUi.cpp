#include "Game.h"

#include <cmath>
#include <cstdio>

#include "DrawHelpers.h"
#include "Theme.h"

// The interface. In play: small dark cards (objective, progress, code line, controls).
// For reading: an explorer's field journal (inspector, notes, pause, and the summary).
// Every list detail shown here is read from the real active list.

namespace {

const Color CODE_BLUE = {150, 230, 255, 255};

// A key cap followed by an action, e.g. [TAB] journal. Returns the width used.
float drawKeyHint(const char* key, const char* action, float x, float y, Color capColor, Color keyColor, Color actionColor) {
    int keyWidth = MeasureText(key, 10) + 10;
    DrawRectangleRounded(Rectangle{x, y, (float)keyWidth, 14.0f}, 0.3f, 4, capColor);
    DrawText(key, (int)(x + 5.0f), (int)(y + 2.0f), 10, keyColor);
    DrawText(action, (int)(x + keyWidth + 6.0f), (int)(y + 2.0f), 10, actionColor);
    return keyWidth + 6.0f + MeasureText(action, 10);
}

float measureKeyHint(const char* key, const char* action) {
    return MeasureText(key, 10) + 10.0f + 6.0f + MeasureText(action, 10);
}

// A sheet of journal paper with a soft shadow and a darker edge.
void drawParchment(Rectangle r) {
    DrawRectangleRounded(Rectangle{r.x + 4.0f, r.y + 5.0f, r.width, r.height}, 0.06f, 6, Fade(BLACK, 0.35f));
    DrawRectangleRounded(r, 0.06f, 6, PARCHMENT);
    DrawRectangleRoundedLinesEx(Rectangle{r.x + 4.0f, r.y + 4.0f, r.width - 8.0f, r.height - 8.0f}, 0.06f, 6, 1.0f,
                                Fade(PARCHMENT_SHADE, 0.9f));
    for (int i = 0; i < 26; i++) { // paper flecks
        int x = (int)(r.x + 8.0f + hash01(i * 7 + (int)r.width) * (r.width - 16.0f));
        int y = (int)(r.y + 8.0f + hash01(i * 13 + (int)r.height) * (r.height - 16.0f));
        DrawRectangle(x, y, 2, 2, Fade(PARCHMENT_SHADE, 0.5f));
    }
}

// A small octagonal stop mark: shown next to every NULL, so NULL is readable without color.
void drawStopMark(float x, float y, Color color) {
    DrawPoly(Vector2{x, y}, 8, 6.0f, 22.5f, color);
    DrawRectangle((int)(x - 3.0f), (int)(y - 1.0f), 6, 2, PARCHMENT);
}

// The mark on a journal note: each kind of note has its own shape.
void drawNoteMark(NoteKind kind, float x, float y, Color ink) {
    DrawCircleV(Vector2{x, y}, 15.0f, Fade(ink, 0.15f));
    DrawRing(Vector2{x, y}, 13.0f, 15.0f, 0.0f, 360.0f, 28, ink);
    switch (kind) {
        case NoteKind::Question: DrawText("?", (int)x - 5, (int)y - 10, 20, ink); break;
        case NoteKind::Answer: DrawText("!", (int)x - 3, (int)y - 10, 20, ink); break;
        case NoteKind::Recovery: // a lantern
            DrawRectangle((int)x - 4, (int)y - 6, 8, 11, ink);
            DrawRectangle((int)x - 2, (int)y - 4, 4, 7, GILT);
            DrawRectangle((int)x - 1, (int)y - 9, 2, 3, ink);
            break;
        case NoteKind::Intro: // a compass star
            DrawPoly(Vector2{x, y}, 4, 9.0f, 0.0f, ink);
            DrawPoly(Vector2{x, y}, 4, 4.0f, 45.0f, GILT);
            break;
        default: // a quill dot
            DrawCircleV(Vector2{x, y}, 4.0f, ink);
            break;
    }
}

// The journal's "walk from head" line is made of tokens such as N1, ->, NULL.
enum class TokenStyle { Plain, Current, Null, LoopEnd, Arrow };

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
        default:
            DrawRectangle((int)x - 14, (int)y, 28, 3, ink);
            DrawRectangle((int)x - 14, (int)y - 8, 2, 11, ink);
            DrawRectangle((int)x + 12, (int)y - 8, 2, 11, ink);
            for (int i = 0; i < 5; i++) DrawRectangle((int)x - 11 + i * 5, (int)y + 3, 3, 2, Fade(ink, 0.6f));
            break;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Frame layout
// ---------------------------------------------------------------------------

void Game::draw() const {
    ClearBackground(BLACK);
    drawScene();

    if (fadeTimer > 0.0f) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, fadeTimer / RECOVERY_FADE_SECONDS));
    }
    if (transitionTarget >= 0) { // fade out, then back in
        float alpha = transitionSwitched
                          ? 1.0f - (transitionTimer - TRANSITION_OUT_SECONDS) / TRANSITION_IN_SECONDS
                          : transitionTimer / TRANSITION_OUT_SECONDS;
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, std::fmin(1.0f, std::fmax(0.0f, alpha))));
    }
    if (state == GameState::Complete) {
        drawCompleteScreen();
        return;
    }

    drawHud();
    drawTitleCard();
    if (inspectorOpen) drawInspector();
    drawToast();
    drawInteractionPrompt();
    if (state == GameState::Playing) drawControlsStrip();
    if (state == GameState::Message) drawMessageBox();
    if (state == GameState::Paused) drawPauseScreen();
}

// ---------------------------------------------------------------------------
// In-play HUD
// ---------------------------------------------------------------------------

// Top left: where you are and what to do.
void Game::drawHud() const {
    const RegionInfo& region = world.getRegion(regionIndex);
    const RegionStyle& style = REGION_STYLES[regionIndex];

    char regionLine[96];
    std::snprintf(regionLine, sizeof(regionLine), "REGION %d  -  %s", regionIndex + 1, TextToUpper(region.placeName));
    char kindLine[64];
    std::snprintf(kindLine, sizeof(kindLine), "%s", TextToUpper(region.title));
    const char* objective = currentObjective();

    int width = MeasureText(regionLine, 10) + 16 + MeasureText(kindLine, 10);
    int objectiveWidth = MeasureText(objective, 20);
    if (objectiveWidth > width) width = objectiveWidth;

    Rectangle card = {12.0f, 10.0f, width + 40.0f, 50.0f};
    DrawRectangleRounded(card, 0.25f, 6, Fade(UI_NAVY, 0.8f));
    DrawRectangleRounded(Rectangle{card.x + 10.0f, card.y + 10.0f, 3.0f, 30.0f}, 1.0f, 4, style.accent);
    DrawText(regionLine, (int)card.x + 22, (int)card.y + 9, 10, style.accent);
    DrawText(kindLine, (int)card.x + 22 + MeasureText(regionLine, 10) + 16, (int)card.y + 9, 10, UI_DIM_TEXT);
    DrawText(objective, (int)card.x + 22, (int)card.y + 22, 20, OBJECTIVE_YELLOW);

    if (!inspectorOpen) drawProgressPips();
}

// Top right: the active list as small marks, walked from head through the real next links.
// The current node is a gold diamond, visited nodes are filled dots, the rest are rings.
// The walk ends at NULL (with a stop mark) or, in a circle, back at the head.
void Game::drawProgressPips() const {
    const RegionStyle& style = REGION_STYLES[regionIndex];
    Node* head = world.getHead(regionIndex);

    int count = 0;
    bool loops = false;
    Node* node = head;
    while (node != nullptr && count < MAX_NODES_PER_REGION) {
        count++;
        node = node->next;
        if (node == head) {
            loops = true; // circular list: the walk stops when it is back at the head
            break;
        }
    }

    const float gap = 18.0f;
    const char* endLabel = loops ? "BACK TO HEAD" : "NULL";
    char steps[64];
    std::snprintf(steps, sizeof(steps), "STEPS %d / MAX %d   STOP WHEN current == N1", loopSteps, MAX_LOOP_STEPS);
    float width = 28.0f + MeasureText("HEAD", 10) + 10.0f + count * gap + 6.0f + MeasureText(endLabel, 10) + (loops ? 14.0f : 16.0f);
    if (regionIndex == 2 && width < MeasureText(steps, 10) + 28.0f) width = MeasureText(steps, 10) + 28.0f;
    Rectangle card = {SCREEN_WIDTH - 12.0f - width, 10.0f, width, (regionIndex == 2) ? 46.0f : 30.0f};
    DrawRectangleRounded(card, 0.3f, 6, Fade(UI_NAVY, 0.8f));

    float x = card.x + 14.0f;
    float y = card.y + 15.0f;
    DrawText("HEAD", (int)x, (int)y - 4, 10, UI_DIM_TEXT);
    x += MeasureText("HEAD", 10) + 10.0f;
    node = head;
    for (int i = 0; i < count && node != nullptr; i++) {
        Vector2 pip = {x + i * gap + 4.0f, y};
        if (i + 1 < count) DrawLineEx(Vector2{pip.x + 6.0f, y}, Vector2{pip.x + gap - 6.0f, y}, 1.0f, Fade(UI_DIM_TEXT, 0.6f));
        bool visited = node->id >= 0 && node->id <= MAX_NODES_PER_REGION && visitedIds[node->id];
        if (node == player.currentNode) {
            DrawPoly(pip, 4, 6.5f, 0.0f, CURRENT_GOLD);
        } else if (visited) {
            DrawCircleV(pip, 3.5f, style.accent);
        } else {
            DrawRing(pip, 2.5f, 4.0f, 0.0f, 360.0f, 16, Fade(UI_DIM_TEXT, 0.9f));
        }
        node = node->next;
    }
    x += count * gap + 6.0f;
    if (loops) {
        DrawRing(Vector2{x + 3.0f, y}, 3.0f, 4.5f, 40.0f, 330.0f, 14, style.accent); // a loop mark
        DrawText(endLabel, (int)x + 12, (int)y - 4, 10, style.accent);
    } else {
        DrawPoly(Vector2{x + 4.0f, y}, 8, 5.0f, 22.5f, NULL_AMBER); // stop mark
        DrawRectangle((int)x + 2, (int)y - 1, 5, 2, UI_NAVY);
        DrawText(endLabel, (int)x + 13, (int)y - 4, 10, NULL_AMBER);
    }
    if (regionIndex == 2) DrawText(steps, (int)card.x + 14, (int)card.y + 30, 10, UI_DIM_TEXT);
}

// The code line for the last move, e.g. "current = current->next;   N2 -> N3".
void Game::drawToast() const {
    if (toastTimer <= 0.0f || toastText.empty()) {
        return;
    }
    float alpha = toastTimer < 0.4f ? toastTimer / 0.4f : 1.0f;
    int width = MeasureText(toastText.c_str(), 20);
    float y = 68.0f;
    DrawRectangleRounded(Rectangle{12.0f, y, width + 30.0f, 30.0f}, 0.3f, 4, Fade(UI_NAVY, 0.85f * alpha));
    DrawRectangle(22, (int)y + 7, 3, 16, Fade(REGION_STYLES[regionIndex].accent, alpha));
    DrawText(toastText.c_str(), 32, (int)y + 5, 20, Fade(CODE_BLUE, alpha));
}

// Region 4: a key hint above the traveler while standing on N3 with its NULL link.
void Game::drawInteractionPrompt() const {
    const Node* current = player.currentNode;
    if (state != GameState::Playing || regionIndex != 3 || stage != 1 || current == nullptr || current->id != 3) {
        return;
    }
    const char* key = inspectorOpen ? "E" : "TAB";
    const char* action = inspectorOpen ? "check & repair N3->next" : "check N3->next in the journal";
    float width = measureKeyHint(key, action) + 14.0f;
    float x = player.position.x + player.size.x / 2.0f - camera.target.x - width / 2.0f;
    float y = player.position.y - 34.0f;
    DrawRectangleRounded(Rectangle{x, y, width, 22.0f}, 0.4f, 4, Fade(UI_NAVY, 0.9f));
    DrawRectangleRoundedLinesEx(Rectangle{x, y, width, 22.0f}, 0.4f, 4, 1.5f, NULL_AMBER);
    drawKeyHint(key, action, x + 7.0f, y + 4.0f, UI_TEXT, OUTLINE_DARK, NULL_AMBER);
}

// Bottom left, small and quiet: the controls during active play.
void Game::drawControlsStrip() const {
    struct Hint { const char* key; const char* action; };
    const Hint hints[] = {
        {"A/D or ARROWS", "move"}, {"SPACE", "jump"}, {"E/ENTER", "interact"}, {"TAB", "journal"}, {"ESC", "pause"},
    };
    float total = 0.0f;
    for (const Hint& hint : hints) total += measureKeyHint(hint.key, hint.action) + 14.0f;
    total -= 14.0f;

    float x = 22.0f;
    float y = SCREEN_HEIGHT - 26.0f;
    DrawRectangleRounded(Rectangle{x - 10.0f, y - 4.0f, total + 20.0f, 22.0f}, 0.5f, 6, Fade(UI_NAVY, 0.6f));
    for (const Hint& hint : hints) {
        x += drawKeyHint(hint.key, hint.action, x, y, UI_TEXT, OUTLINE_DARK, UI_DIM_TEXT) + 14.0f;
    }
}

// The region's name, shown briefly after arriving.
void Game::drawTitleCard() const {
    if (titleCardTimer <= 0.0f) {
        return;
    }
    float shown = TITLE_CARD_SECONDS - titleCardTimer;
    float alpha = std::fmin(1.0f, shown / 0.5f) * std::fmin(1.0f, titleCardTimer / 0.7f);
    const RegionInfo& region = world.getRegion(regionIndex);
    const RegionStyle& style = REGION_STYLES[regionIndex];

    char number[32];
    std::snprintf(number, sizeof(number), "REGION %d", regionIndex + 1);
    int y = 150;
    int nameWidth = MeasureText(region.placeName, 40);
    DrawRectangleGradientH(SCREEN_WIDTH / 2 - nameWidth, y - 18, nameWidth, 110, Fade(BLACK, 0.0f), Fade(BLACK, 0.45f * alpha));
    DrawRectangleGradientH(SCREEN_WIDTH / 2, y - 18, nameWidth, 110, Fade(BLACK, 0.45f * alpha), Fade(BLACK, 0.0f));
    DrawText(number, SCREEN_WIDTH / 2 - MeasureText(number, 20) / 2, y, 20, Fade(GILT, alpha));
    DrawText(region.placeName, SCREEN_WIDTH / 2 - nameWidth / 2 + 2, y + 28, 40, Fade(BLACK, 0.6f * alpha));
    DrawText(region.placeName, SCREEN_WIDTH / 2 - nameWidth / 2, y + 26, 40, Fade(PARCHMENT, alpha));
    DrawText(region.title, SCREEN_WIDTH / 2 - MeasureText(region.title, 20) / 2, y + 74, 20, Fade(style.accent, alpha));
}

// ---------------------------------------------------------------------------
// The field journal: the current node's real links, then a walk from the head.
// ---------------------------------------------------------------------------

void Game::drawInspector() const {
    const RegionInfo& region = world.getRegion(regionIndex);
    const RegionStyle& style = REGION_STYLES[regionIndex];
    Node* head = world.getHead(regionIndex);
    const Node* current = player.currentNode;
    bool isDoubly = region.kind == ListKind::Doubly;
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
        count = addToken(tokens, count, capacity, id, node == current ? TokenStyle::Current : TokenStyle::Plain);
        Node* next = node->next;
        steps++;
        if (next == nullptr) {
            count = addToken(tokens, count, capacity, "->", TokenStyle::Arrow);
            count = addToken(tokens, count, capacity, "NULL", TokenStyle::Null);
            stopNote = "stopped: next == NULL (the end of the list)";
            break;
        }
        if (next == head) { // circular list: the stop condition fires
            std::snprintf(id, sizeof(id), "N%d", head->id);
            count = addToken(tokens, count, capacity, "->", TokenStyle::Arrow);
            count = addToken(tokens, count, capacity, id, TokenStyle::LoopEnd);
            stopNote = "stopped: back at the head (node == head)";
            break;
        }
        bool twoWay = isDoubly && next->previous == node;
        count = addToken(tokens, count, capacity, twoWay ? "<->" : "->", TokenStyle::Arrow);
        node = next;
    }

    const char* note = nullptr;
    if (current != nullptr) {
        if (regionIndex == 3 && stage == 1 && current->next == nullptr) {
            note = "next is NULL: there is no node to follow. Press E to append one.";
        } else if (isDoubly && current->previous == nullptr) {
            note = "previous is NULL: this is the head.";
        } else if (current->next == nullptr) {
            note = "next is NULL: this is the tail.";
        } else if (region.kind == ListKind::Circular && current->next == head) {
            note = "next is the head: this is the tail of the loop.";
        }
    }

    const float width = 412.0f;
    const float x = SCREEN_WIDTH - width - 10.0f;
    const float pad = 18.0f;
    const float valueX = x + pad + 134.0f;
    const float rightX = x + width - pad;
    int walkLines = layoutTokens(tokens, count, x + pad, 0.0f, rightX, false, style.ink);
    float height = 40.0f + 24.0f + 16.0f + 26.0f + 26.0f + 30.0f + 12.0f + 16.0f + walkLines * 26.0f + 16.0f +
                   (note != nullptr ? 18.0f : 0.0f) + 12.0f;
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
        DrawPoly(Vector2{x + pad + 6.0f, y + 9.0f}, 4, 7.0f, 0.0f, CURRENT_GOLD); // the same diamond as the HUD
        DrawText(text, (int)(x + pad + 18.0f), (int)y, 20, INK);
        y += 26.0f;

        // next: every list kind stores it.
        describeNode(text, sizeof(text), current->next);
        DrawText("next", (int)(x + pad), (int)y, 20, style.ink);
        DrawText("->", (int)(valueX - 30.0f), (int)y, 20, INK_SOFT);
        DrawText(text, (int)valueX, (int)y, 20, current->next != nullptr ? INK : INK_NULL);
        if (current->next == nullptr) drawStopMark(valueX + MeasureText(text, 20) + 10.0f, y + 9.0f, INK_NULL);
        else if (!emphasizePrevious) DrawText("follow", (int)(rightX - MeasureText("follow", 10)), (int)y + 6, 10, style.ink);
        y += 26.0f;

        // previous: only the doubly linked list stores it.
        if (isDoubly) {
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
    DrawText("WALK FROM HEAD", (int)(x + pad), (int)y, 10, INK_SOFT);
    y += 16.0f;
    layoutTokens(tokens, count, x + pad, y, rightX, true, style.ink);
    y += walkLines * 26.0f;
    DrawText(stopNote, (int)(x + pad), (int)y, 10, INK_SOFT);
    y += 16.0f;
    if (note != nullptr) {
        DrawText(note, (int)(x + pad), (int)y, 10, (regionIndex == 3) ? INK_NULL : INK);
    }
}

// ---------------------------------------------------------------------------
// Journal notes, pause, and the final summary
// ---------------------------------------------------------------------------

// A note pinned near the bottom of the screen. It closes after its last page.
void Game::drawMessageBox() const {
    if (pageIndex >= pages.size()) return;
    const std::string& text = pages[pageIndex];
    NoteKind kind = (pageIndex < pageKinds.size()) ? pageKinds[pageIndex] : NoteKind::Info;
    const RegionStyle& style = REGION_STYLES[regionIndex];

    int lines = 1;
    for (char c : text) {
        if (c == '\n') lines++;
    }
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

// The journal's leather cover, closed while the game is paused.
void Game::drawPauseScreen() const {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.55f));
    Rectangle cover = {SCREEN_WIDTH / 2.0f - 220.0f, 220.0f, 440.0f, 220.0f};
    DrawRectangleRounded(Rectangle{cover.x + 5.0f, cover.y + 6.0f, cover.width, cover.height}, 0.08f, 6, Fade(BLACK, 0.4f));
    DrawRectangleRounded(cover, 0.08f, 6, LEATHER);
    DrawRectangleRoundedLinesEx(Rectangle{cover.x + 10.0f, cover.y + 10.0f, cover.width - 20.0f, cover.height - 20.0f}, 0.08f,
                                6, 2.0f, GILT);
    DrawRectangle((int)cover.x + 28, (int)cover.y, 12, (int)cover.height, LEATHER_DARK); // the spine strap

    const char* title = "PAUSED";
    DrawText(title, SCREEN_WIDTH / 2 - MeasureText(title, 40) / 2, (int)cover.y + 44, 40, GILT);
    const char* subtitle = TextFormat("Region %d - %s", regionIndex + 1, world.getRegion(regionIndex).placeName);
    DrawText(subtitle, SCREEN_WIDTH / 2 - MeasureText(subtitle, 20) / 2, (int)cover.y + 94, 20, PARCHMENT);

    float total = measureKeyHint("ESC", "resume") + 30.0f + measureKeyHint("Q", "quit");
    float x = SCREEN_WIDTH / 2.0f - total / 2.0f;
    x += drawKeyHint("ESC", "resume", x, cover.y + 140.0f, PARCHMENT, LEATHER_DARK, PARCHMENT) + 30.0f;
    drawKeyHint("Q", "quit", x, cover.y + 140.0f, PARCHMENT, LEATHER_DARK, PARCHMENT);
    const char* tip = "Tab opens the field journal at any time.";
    DrawText(tip, SCREEN_WIDTH / 2 - MeasureText(tip, 10) / 2, (int)cover.y + 176, 10, Fade(PARCHMENT, 0.7f));
}

// The journal opened to its last spread: what each region taught.
void Game::drawCompleteScreen() const {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.6f));
    Rectangle book = {120.0f, 70.0f, 1040.0f, 580.0f};
    DrawRectangleRounded(Rectangle{book.x - 14.0f, book.y - 14.0f, book.width + 28.0f, book.height + 28.0f}, 0.04f, 6, LEATHER);
    Rectangle left = {book.x, book.y, book.width / 2.0f - 6.0f, book.height};
    Rectangle right = {book.x + book.width / 2.0f + 6.0f, book.y, book.width / 2.0f - 6.0f, book.height};
    drawParchment(left);
    drawParchment(right);
    DrawRectangle((int)(book.x + book.width / 2.0f - 4.0f), (int)book.y, 8, (int)book.height, LEATHER_DARK); // the spine

    const char* title = "The Wanderer's Journal";
    DrawText(title, (int)(left.x + left.width / 2.0f) - MeasureText(title, 30) / 2, (int)left.y + 40, 30, INK);
    const char* subtitle = "Tutorial stage complete";
    DrawText(subtitle, (int)(left.x + left.width / 2.0f) - MeasureText(subtitle, 20) / 2, (int)left.y + 82, 20, INK_SOFT);

    // A portrait of the traveler, drawn with the same sprite as in the game, four times larger.
    Player portrait = player;
    portrait.grounded = true;
    portrait.velocity = Vector2{0.0f, 0.0f};
    portrait.landTimer = 0.0f;
    portrait.facing = 1.0f;
    Vector2 portraitFeet = {left.x + left.width / 2.0f, left.y + 320.0f};
    DrawEllipse((int)portraitFeet.x, (int)portraitFeet.y + 6, 70.0f, 10.0f, Fade(PARCHMENT_SHADE, 0.9f));
    drawTraveler(portrait, elapsed, portraitFeet, 4.0f);

    const char* journey = "Four lists, four places, one journey.";
    DrawText(journey, (int)(left.x + left.width / 2.0f) - MeasureText(journey, 20) / 2, (int)left.y + 352, 20, INK);
    char nodes[96];
    std::snprintf(nodes, sizeof(nodes), "Nodes still allocated: %d. Each list frees its own nodes, once, on exit.",
                  g_liveNodeCount);
    DrawText(nodes, (int)(left.x + left.width / 2.0f) - MeasureText(nodes, 10) / 2, (int)left.y + 392, 10, INK_SOFT);

    struct Entry { const char* line1; const char* line2; };
    const Entry entries[REGION_COUNT] = {
        {"Follow next in one direction.", "The tail's next is NULL."},
        {"next and previous go both ways.", "head->previous, tail->next: NULL."},
        {"tail->next is the head.", "Stop at the start, or after N steps."},
        {"Check a pointer before you follow it.", "nullptr means there is no node."},
    };
    for (int i = 0; i < REGION_COUNT; i++) {
        const RegionInfo& region = world.getRegion(i);
        const RegionStyle& style = REGION_STYLES[i];
        float y = right.y + 34.0f + i * 112.0f;
        float x = right.x + 34.0f;
        drawRegionSketch(i, x + 14.0f, y + 16.0f, style.ink);
        DrawText(TextFormat("%d  %s", i + 1, region.placeName), (int)x + 44, (int)y, 20, style.ink);
        DrawText(region.title, (int)x + 44, (int)y + 26, 10, INK_SOFT);
        DrawText(entries[i].line1, (int)x + 44, (int)y + 46, 20, INK);
        DrawText(entries[i].line2, (int)x + 44, (int)y + 70, 20, INK);
    }

    float total = measureKeyHint("R", "replay") + 40.0f + measureKeyHint("Q / ESC", "exit");
    float x = left.x + left.width / 2.0f - total / 2.0f;
    float y = left.y + left.height - 60.0f;
    x += drawKeyHint("R", "replay", x, y, INK, PARCHMENT, INK) + 40.0f;
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
}

void Game::showToast(const std::string& text, float seconds) {
    toastText = text;
    toastTimer = seconds;
}

const char* Game::currentObjective() const {
    switch (regionIndex) {
        case 0:
            return gateOpen ? "Done! Walk into the arch on N5." : "Follow next from the head N1 to the tail N5.";
        case 1:
            if (gateOpen) return "Done! Walk through the doorway on N5.";
            if (stage == 0) return "Go forward to N4 (Key Shrine) and take the key.";
            if (stage == 1) return "Carry its light back to N2 (Old Hall) through previous.";
            return "Continue forward to the tail N5.";
        case 2:
            return gateOpen ? "Back at the start! Walk into the moon gate on N1."
                            : "Follow next around the loop back to the start N1.";
        case 3:
            if (gateOpen) return "Stage clear! Walk into the dawn gate.";
            if (stage == 0) return "Reach N3 (Edge) and check its next link.";
            if (stage == 1) return "N3->next is NULL. Open the journal (Tab), then press E.";
            return "Cross the new node N4 (Bridge) to the dawn gate.";
        default:
            return "";
    }
}
