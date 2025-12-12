#include <raylib.h>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <cmath>
#include "../storage/storage.h"
#include "../query/parser.h"
#include "../utils/helpers.h"
#include "../graph/graph.h"

using namespace std;

// Better Text Box
struct TextBox {
    Rectangle rect;
    string text;
    bool active;
    int cursorFrames;
    int cursorPos; // Track cursor position
};

void DrawTextBox(TextBox& box) {
    // Background and Border
    Color borderColor = box.active ? SKYBLUE : LIGHTGRAY;
    Color bgColor = box.active ? Fade(SKYBLUE, 0.1f) : Fade(LIGHTGRAY, 0.1f);
    
    DrawRectangleRec(box.rect, bgColor);
    DrawRectangleLinesEx(box.rect, 2, borderColor);

    // Text with better Font
    DrawText(box.text.c_str(), (int)box.rect.x + 10, (int)box.rect.y + 10, 20, DARKGRAY);

    // Blinking Cursor
    if (box.active) {
        box.cursorFrames++;
        if ((box.cursorFrames / 30) % 2 == 0) {
            // Calculate pixel position of cursor
            string textBeforeCursor = box.text.substr(0, box.cursorPos);
            int textWidth = MeasureText(textBeforeCursor.c_str(), 20);
            DrawRectangle((int)box.rect.x + 10 + textWidth, (int)box.rect.y + 10, 2, 20, BLACK);
        }
    }
}

// Helper to split lines for coloring
vector<string> SplitLines(const string& str) {
    vector<string> lines;
    stringstream ss(str);
    string line;
    while (getline(ss, line)) {
        lines.push_back(line);
    }
    return lines;
}

// ... (helper functions)

int main() {
    const int screenWidth = 1400; // Wider for sidebar
    const int screenHeight = 900;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "ChronoDB Studio");
    SetTargetFPS(60);

    // Database Setup
    ChronoDB::StorageEngine storage("data");
    GraphEngine graph; 
    ChronoDB::Parser parser(storage, graph);

    // -- Visual State --
    bool showGraph = false;
    string currentGraphName = "";
    // Node Positions Cache (to avoid recalc every frame if we wanted, but immediate mode is fine for small graphs)
    // Structure: Name -> Vector2
    
    // UI State
    TextBox queryBox = { { 250, 60, 900, 50 }, "", false, 0, 0 }; // Init cursorPos = 0
    Rectangle btnRect = { 1170, 60, 180, 50 };
    Rectangle sidebarRect = { 0, 0, 230, (float)screenHeight };
    Rectangle metaRect = { 250, 600, 1100, 280 }; // Bottom Metadata panel

    bool btnHover = false;
    float scrollOffset = 0.0f;
    float contentHeight = 0.0f;

    // Data Caching
    vector<string> tableList = storage.getTableNames();
    string selectedTable = "";
    vector<ChronoDB::Column> selectedColumns;

    // Output Log
    vector<string> logLines;
    logLines.push_back("Welcome to ChronoDB Studio!");
    logLines.push_back("Type 'CREATE TABLE...' to see updates here.");

    while (!WindowShouldClose()) {
        Vector2 mouse = GetMousePosition();
        
        // Refresh table list occasionally (e.g., if execution happened)
        // For simplicity, we just do it if execute happened, see below.

        // Input & Button Logic (Unchanged but shifted)
        if (CheckCollisionPointRec(mouse, queryBox.rect)) {
            SetMouseCursor(MOUSE_CURSOR_IBEAM);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) queryBox.active = true;
        } else {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) queryBox.active = false;
        }

        if (queryBox.active) {
            // 1. Handle Typing
            int key = GetCharPressed();
            while (key > 0) {
                if ((key >= 32) && (key <= 125)) {
                    queryBox.text.insert(queryBox.cursorPos, 1, (char)key);
                    queryBox.cursorPos++;
                }
                key = GetCharPressed();
            }

            // 2. Handle Backspace
            if (IsKeyPressed(KEY_BACKSPACE)) {
                if (queryBox.cursorPos > 0 && !queryBox.text.empty()) {
                    queryBox.text.erase(queryBox.cursorPos - 1, 1);
                    queryBox.cursorPos--;
                }
            }
            
            // 3. Handle Delete (Forward Delete)
            if (IsKeyPressed(KEY_DELETE)) {
                if (queryBox.cursorPos < (int)queryBox.text.length()) {
                    queryBox.text.erase(queryBox.cursorPos, 1);
                }
            }

            // 4. Handle Arrow Keys
            if (IsKeyPressed(KEY_LEFT)) {
                if (queryBox.cursorPos > 0) queryBox.cursorPos--;
            }
            if (IsKeyPressed(KEY_RIGHT)) {
                if (queryBox.cursorPos < (int)queryBox.text.length()) queryBox.cursorPos++;
            }
            
            // 5. Handle Home/End
            if (IsKeyPressed(KEY_HOME)) queryBox.cursorPos = 0;
            if (IsKeyPressed(KEY_END)) queryBox.cursorPos = (int)queryBox.text.length();

            // 6. Handle Clipboard (Ctrl+V)
            if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) {
                const char* clipText = GetClipboardText();
                if (clipText != NULL) {
                    string s(clipText);
                    queryBox.text.insert(queryBox.cursorPos, s);
                    queryBox.cursorPos += (int)s.length();
                }
            }
        }

        btnHover = CheckCollisionPointRec(mouse, btnRect);
        bool execute = false;
        if ((btnHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) || (queryBox.active && IsKeyPressed(KEY_ENTER))) {
            execute = true;
        }

        if (execute && !queryBox.text.empty()) {
            stringstream ssFull(queryBox.text);
            string line;
            while(getline(ssFull, line, '\n')) {
                // Remove \r for Windows
                if (!line.empty() && line.back() == '\r') line.pop_back();
                
                // Trim leading/trailing whitespace
                size_t first = line.find_first_not_of(" \t");
                if (first == string::npos) continue; // Skip empty lines
                
                string cmd = line.substr(first);
                size_t last = cmd.find_last_not_of(" \t");
                if (last != string::npos) cmd = cmd.substr(0, last + 1);

                if (cmd.empty()) continue;

                // INTERCEPT GRAPH SHOW
                string cmdUpper = cmd; 
                for(auto & c: cmdUpper) c = toupper(c);

                if (cmdUpper.rfind("GRAPH SHOW", 0) == 0) { // Starts with GRAPH SHOW
                     stringstream ss(cmd);
                     string temp, gName;
                     ss >> temp >> temp >> gName; // GRAPH SHOW <gName>
                     
                     // Remove semicolon or other trailing junk (spaces, \r, etc)
                     while (!gName.empty() && !isalnum(gName.back())) {
                         gName.pop_back();
                     }

                     if (!gName.empty()) {
                         currentGraphName = gName;
                         showGraph = true;
                         logLines.push_back("> " + cmd);
                         logLines.push_back("Opening Graph View for " + gName + "...");
                     } else {
                         logLines.push_back("> " + cmd);
                         logLines.push_back("[ERROR] Usage: GRAPH SHOW <name>");
                     }
                } 
                else {
                    Helper::startCapture();
                    parser.parseAndExecute(cmd);
                    string rawResult = Helper::stopCapture();

                    logLines.push_back("> " + cmd);
                    auto lines = SplitLines(rawResult);
                    logLines.insert(logLines.end(), lines.begin(), lines.end());
                    logLines.push_back(""); 
                    
                    // Refresh Table List if needed
                    tableList = storage.getTableNames();
                    if (!selectedTable.empty() && !storage.tableExists(selectedTable)) {
                        selectedTable = "";
                        selectedColumns.clear();
                    }
                }
            }
            
            queryBox.text = ""; 
            queryBox.cursorPos = 0; 
            scrollOffset = -100000; 
        }

        // Sidebar Logic
        if (CheckCollisionPointRec(mouse, sidebarRect)) {
             // Basic list click detection
             float y = 50;
             for (const auto& tb : tableList) {
                 Rectangle itemRect = { 10, y, 210, 30 };
                 if (CheckCollisionPointRec(mouse, itemRect)) {
                     if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                         selectedTable = tb;
                         selectedColumns = storage.getTableColumns(tb);
                     }
                 }
                 y += 35;
             }
        }

        // Scrolling Logic (Unchanged)
        Rectangle outRect = { 250, 140, 1100, 440 }; // Shrunk height to fit Metadata
        float wheel = GetMouseWheelMove();
        if (CheckCollisionPointRec(mouse, outRect) && wheel != 0) scrollOffset += wheel * 60.0f; // Faster Scroll
        contentHeight = logLines.size() * 24.0f + 50;
        
        // Clamping Logic
        float minScroll = -(contentHeight - outRect.height);
        if (minScroll > 0) minScroll = 0; // Content fits, no scroll needed
        
        if (scrollOffset > 0) scrollOffset = 0;              // Top bound
        else if (scrollOffset < minScroll) scrollOffset = minScroll; // Bottom bound

        if (showGraph) {
            // --- GRAPH VIEW ---
            BeginDrawing();
            ClearBackground(RAYWHITE);
            
            DrawText(("Viewing Graph: " + currentGraphName).c_str(), 20, 20, 30, DARKBLUE);
            
            Graph* g = graph.getGraph(currentGraphName);
            if (g) {
                const auto& adj = g->getAdjacencyList();
                if (adj.empty()) {
                    DrawText("Graph is empty.", 600, 400, 30, GRAY);
                } else {
                    // Circular Layout
                    int nodeCount = adj.size();
                    float centerX = screenWidth / 2.0f;
                    float centerY = screenHeight / 2.0f;
                    float radius = 300.0f;
                    float angleStep = 360.0f / nodeCount;
                    
                    unordered_map<string, Vector2> positions;
                    int i = 0;
                    for(const auto& pair : adj) {
                        float rad = (angleStep * i) * (PI / 180.0f);
                        float x = centerX + radius * cos(rad);
                        float y = centerY + radius * sin(rad);
                        positions[pair.first] = {x, y};
                        i++;
                    }
                    
                    // Draw Edges first (so they are behind nodes)
                    for(const auto& pair : adj) {
                        Vector2 start = positions[pair.first];
                        for(const auto& neighbor : pair.second) {
                             if (positions.find(neighbor.first) != positions.end()) {
                                 Vector2 end = positions[neighbor.first];
                                 DrawLineEx(start, end, 2.0f, DARKGRAY);
                                 
                                 // Weight Label in middle
                                 Vector2 mid = { (start.x + end.x)/2, (start.y + end.y)/2 };
                                 DrawText(to_string(neighbor.second).c_str(), mid.x, mid.y, 10, RED); 
                             }
                        }
                    }
                    
                    // Draw Nodes
                    for(const auto& pair : positions) {
                        DrawCircleV(pair.second, 30, SKYBLUE);
                        DrawCircleLines((int)pair.second.x, (int)pair.second.y, 30, BLUE);
                        int textW = MeasureText(pair.first.c_str(), 20);
                        DrawText(pair.first.c_str(), pair.second.x - textW/2, pair.second.y - 10, 20, DARKBLUE);
                    }
                }
                
            } else {
                 DrawText("Graph not found!", 600, 400, 30, RED);
            }

            // Close Button
            Rectangle closeBtn = { 20, 70, 100, 40 };
            if (CheckCollisionPointRec(mouse, closeBtn)) {
                 DrawRectangleRec(closeBtn, RED);
                 if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) showGraph = false;
            } else {
                 DrawRectangleRec(closeBtn, MAROON);
            }
            DrawText("CLOSE", 35, 80, 20, WHITE);

            EndDrawing();
        } 
        else {
            // --- STANDARD VIEW ---
            BeginDrawing();
            ClearBackground(RAYWHITE);

            // Sidebar Background
            DrawRectangleRec(sidebarRect, Color{40, 44, 52, 255}); // Darkest
            DrawText("TABLES", 20, 15, 20, LIGHTGRAY);
            DrawLine(10, 40, 220, 40, GRAY);
            // ... (Rest of sidebar drawing)
            float listY = 50;
            for (const auto& tb : tableList) {
                Color textColor = (tb == selectedTable) ? SKYBLUE : WHITE;
                if (CheckCollisionPointRec(mouse, {10, listY, 210, 30})) textColor = YELLOW;
                DrawText(tb.c_str(), 20, (int)listY + 5, 20, textColor);
                listY += 35;
            }

            // Header
            DrawText("ChronoDB Studio", 260, 15, 30, DARKBLUE);
            DrawText("v1.1", 520, 25, 10, GRAY);

            // Input & Button
            DrawTextBox(queryBox);
            DrawRectangleRec(btnRect, btnHover ? SKYBLUE : BLUE);
            DrawText("RUN", (int)btnRect.x + 65, (int)btnRect.y + 15, 20, WHITE);

            // Console
            DrawRectangleRec(outRect, GetColor(0x1e1e1eff)); 
            BeginScissorMode((int)outRect.x, (int)outRect.y, (int)outRect.width, (int)outRect.height);
                int startY = (int)outRect.y + 10 + (int)scrollOffset;
                for (const auto& line : logLines) {
                    Color lineColor = LIGHTGRAY;
                    if (line.find("[SUCCESS]") != string::npos) lineColor = GREEN;
                    else if (line.find("[ERROR]") != string::npos) lineColor = RED;
                    else if (line.find(">") == 0) lineColor = YELLOW;
                    else if (line.find("+") == 0 || line.find("|") == 0) lineColor = WHITE;

                    if (startY > outRect.y - 30 && startY < outRect.y + outRect.height) {
                        DrawText(line.c_str(), (int)outRect.x + 15, startY, 20, lineColor);
                    }
                    startY += 24;
                }
            EndScissorMode();

            // METADATA PANEL
            if (!selectedTable.empty()) {
                DrawRectangleRec(metaRect, Fade(SKYBLUE, 0.2f));
                DrawRectangleLinesEx(metaRect, 2, BLUE);
                DrawText(("Schema: " + selectedTable).c_str(), (int)metaRect.x + 10, (int)metaRect.y + 10, 20, DARKBLUE);
                
                int colX = (int)metaRect.x + 20;
                int colY = (int)metaRect.y + 40;
                for (const auto& col : selectedColumns) {
                    string info = col.name + " (" + col.type + ")";
                    DrawRectangle(colX - 5, colY - 2, 200, 24, WHITE);
                    DrawText(info.c_str(), colX, colY, 20, BLACK);
                    colY += 30;
                    if (colY > metaRect.y + metaRect.height - 30) {
                        colY = (int)metaRect.y + 40;
                        colX += 220; // Wrap columns
                    }
                }
            } else {
                DrawText("Select a table to view schema", (int)metaRect.x + 400, (int)metaRect.y + 130, 20, GRAY);
            }

            EndDrawing();
        }
    }

    CloseWindow();
    return 0;
}
