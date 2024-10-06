#include <SFML/Graphics.hpp>
#include "../include/Globals.h"
#include <cmath>

enum DisplayMode : unsigned int {
    GRID = 0,
    CONTOUR = 1,
    INTERPOLATING_CONTOUR = 2
};

enum BrushMode : unsigned int {
    ADDING = 0,
    REMOVING = 1,
};

sf::Vector2f getRelPosSide(int side){
    switch(side){
        case 0:
            return sf::Vector2f(TILE_LENGTH * 0.5, 0);
        case 1:
            return sf::Vector2f(TILE_LENGTH, TILE_LENGTH * 0.5);
        case 2:
            return sf::Vector2f(TILE_LENGTH * 0.5, TILE_LENGTH);
        case 3:
            return sf::Vector2f(0, TILE_LENGTH * 0.5);
        default:
            exit(-1);
    }
}

void drawDisplayMode(sf::RenderWindow& window, DisplayMode mode, float threshold) {
    static sf::Font font;
    static bool fontLoaded = false;

    // Load the font (only once)
    if (!fontLoaded) {
        if (!font.loadFromFile("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf")) {
            printf("Error loading font\n");
            return;
        }
        fontLoaded = true;
    }

    // Mode text
    sf::Text modeText;
    modeText.setFont(font);
    modeText.setCharacterSize(20);
    modeText.setFillColor(sf::Color::Black);
    modeText.setPosition(10, 10);

    // Set the string based on the mode
    switch (mode) {
        case DisplayMode::CONTOUR:
            modeText.setString("Mode: CONTOUR");
            break;
        case DisplayMode::GRID:
            modeText.setString("Mode: GRID");
            break;
        case DisplayMode::INTERPOLATING_CONTOUR:
            modeText.setString("Mode: INTERPOLATING CONTOUR");
            break;
    }

    // Threshold text
    sf::Text thresholdText;
    thresholdText.setFont(font);
    thresholdText.setCharacterSize(20);
    thresholdText.setFillColor(sf::Color::Black);
    thresholdText.setPosition(10, 40);  // Position it below the mode text

    // Set the threshold string with 2 decimal places
    char thresholdStr[50];
    snprintf(thresholdStr, sizeof(thresholdStr), "Threshold: %.2f", threshold);
    thresholdText.setString(thresholdStr);

    // Draw both texts
    window.draw(modeText);
    window.draw(thresholdText);
}

sf::Vector2f getRelPosSide(int side, float* grid, int x, int y, int nX, float threshold){

    float fx0 = -1;
    float fx1 = -1;

    sf::Vector2f x0;
    sf::Vector2f x1;

    float t = -1.;
    
    switch(side){
        case 0:

            x0 = sf::Vector2f(0, 0);
            x1 = sf::Vector2f(TILE_LENGTH, 0);

            fx0 = grid[y * nX  + x];
            fx1 = grid[y * nX + (x + 1)];
            
            break;

        case 1:
            
            x0 = sf::Vector2f(TILE_LENGTH, 0);
            x1 = sf::Vector2f(TILE_LENGTH, TILE_LENGTH);

            fx0 = grid[y * nX + (x + 1)];
            fx1 = grid[(y + 1) * nX  + (x + 1)];
            
            break;

        case 2:
            
            x0 = sf::Vector2f(TILE_LENGTH, TILE_LENGTH);
            x1 = sf::Vector2f(0, TILE_LENGTH);

            fx0 = grid[(y + 1) * nX + (x + 1)];
            fx1 = grid[(y + 1) * nX + x];
            
            break;

        case 3:
            
            x0 = sf::Vector2f(0, TILE_LENGTH);
            x1 = sf::Vector2f(0, 0);

            fx0 = grid[(y + 1) * nX + x];
            fx1 = grid[y * nX + x];
            
            break;
        
        default:
            exit(-1);
    }

    t = (threshold - fx0) / (fx1 - fx0);

    // clamping
    t = t > 1 ? 1 : (t < 0 ? 0 : t);

    return (1.f - t) * x0 + t * x1;
}

float length(sf::Vector2f& v){
    return std::sqrt(v.x * v.x + v.y * v.y);
}

float clamp(float x, float min, float max){
    return x > max ? max : (x < min ? min : x);
}

int main()
{
    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Marching Squares");

    window.setMouseCursorVisible(false);

    bool mousePressed = false;
    
    sf::Vector2i mousePos;

    int nY = WINDOW_HEIGHT / TILE_LENGTH;
    int nX = WINDOW_WIDTH / TILE_LENGTH;
    float grid[nY][nX];

    for(int y = 0; y < nY; y++){
        for(int x = 0; x < nX; x++){
            grid[y][x] = 0;
        }
    }

    DisplayMode displayMode = GRID;
    BrushMode brushMode = ADDING;

    bool gridHasChanged = true;
    std::vector<int> cellBeginsAtIndex;
    std::vector<int> cellHasSides;
    std::vector<int> sides;

    // for marching squares
    float threshold = 0.5;

    // for drawing
    float kernelRadius = 30.;

    float minKernelRadius = 10;
    float maxKernelRadius = 50;

    int maxKernelRadiusInTileLengths= (int)((maxKernelRadius / TILE_LENGTH) + 1);

    float kernel[2 * maxKernelRadiusInTileLengths + 1][2 * maxKernelRadiusInTileLengths + 1];

    sf::Vector2f midPoint(kernelRadius, kernelRadius);

    for(int i = 0; i < 2 * maxKernelRadiusInTileLengths + 1; i++){
        for(int j = 0; j < 2 * maxKernelRadiusInTileLengths + 1; j++){
            
            sf::Vector2f position((j + 0.5) * TILE_LENGTH, (i + 0.5) * TILE_LENGTH);
            sf::Vector2f distVec = position - midPoint;

            float lengthToMidPoint = length(distVec);

            //printf("length to midpoint: %.3f\n", lengthToMidPoint);
            kernel[i][j] = clamp(1.0 - (lengthToMidPoint / kernelRadius), 0.0f, 1.0f);
        }
    }

    sf::Clock clock;

    int lastMicroSeconds = clock.getElapsedTime().asMicroseconds();

    float deltaTime = 0;

    // half a second till completely filled
    float timeUntilCompletelyFilled = 0.1;
    float timeUntilCompletelyRemoved = 0.01;

    float speedFactorThreshold = 1;

    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();

            mousePos = sf::Mouse::getPosition(window);

            if (event.type == sf::Event::MouseButtonPressed) {

                if (event.mouseButton.button == sf::Mouse::Left) {
                    brushMode = ADDING;
                    mousePressed = true;
                }
                else if (event.mouseButton.button == sf::Mouse::Right) {
                    brushMode = REMOVING;
                    mousePressed = true;
                }
            }

            if (event.type == sf::Event::MouseButtonReleased) {

                if (event.mouseButton.button == sf::Mouse::Left || event.mouseButton.button == sf::Mouse::Right) {
                    mousePressed = false;
                }
            }

            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::C) {
                    displayMode = CONTOUR;
                }
                if (event.key.code == sf::Keyboard::G) {
                    displayMode = GRID;
                }
                if (event.key.code == sf::Keyboard::I) {
                    displayMode = INTERPOLATING_CONTOUR;
                }
                if (event.key.code == sf::Keyboard::Up) {

                    threshold += (threshold + speedFactorThreshold * deltaTime <= 1.0 ? speedFactorThreshold * deltaTime : 0);

                    // needs to recompute contour at different level
                    gridHasChanged = true;
                }
                if (event.key.code == sf::Keyboard::Down) {
                    threshold -= (threshold - speedFactorThreshold * deltaTime >= 0 ? speedFactorThreshold * deltaTime : 0);

                    // needs to recompute contour at different level
                    gridHasChanged = true;
                }
            }

            if (event.type == sf::Event::MouseWheelScrolled)
            {
                if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel)
                {                    
                    if (event.mouseWheelScroll.delta > 0){
                        kernelRadius *= 1.1;
                    }

                    else if (event.mouseWheelScroll.delta < 0){
                        kernelRadius *= 0.9;
                    }

                    kernelRadius = clamp(kernelRadius, minKernelRadius, maxKernelRadius);

                    // TODO: encapsulate into function
                    // recompute kernel
                    sf::Vector2f midPoint(kernelRadius, kernelRadius);

                    for(int i = 0; i < 2 * maxKernelRadiusInTileLengths + 1; i++){
                        for(int j = 0; j < 2 * maxKernelRadiusInTileLengths + 1; j++){
                            
                            sf::Vector2f position((j + 0.5) * TILE_LENGTH, (i + 0.5) * TILE_LENGTH);
                            sf::Vector2f distVec = position - midPoint;

                            float lengthToMidPoint = length(distVec);

                            //printf("length to midpoint: %.3f\n", lengthToMidPoint);
                            kernel[i][j] = clamp(1.0 - (lengthToMidPoint / kernelRadius), 0.0f, 1.0f);
                        }
                    }
                }
            }
        }

        int currentMicroSeconds = clock.getElapsedTime().asMicroseconds();

        int microSecondsSinceLastFrame = currentMicroSeconds - lastMicroSeconds;
        lastMicroSeconds = currentMicroSeconds;

        deltaTime = microSecondsSinceLastFrame * 1e-6;

        // update the grid if possible
        if(mousePressed){

            int mouseGridX = (int)roundf(mousePos.x / TILE_LENGTH);
            int mouseGridY = (int)roundf(mousePos.y / TILE_LENGTH);

            // simple kernel function
            float target = brushMode == ADDING ? 1.0f : 0.0f;
            float strength = brushMode == ADDING ? (deltaTime / timeUntilCompletelyFilled) : (deltaTime / timeUntilCompletelyRemoved);
            
            for(int i = -maxKernelRadiusInTileLengths; i <= maxKernelRadiusInTileLengths; i++){
                for(int j = -maxKernelRadiusInTileLengths; j <= maxKernelRadiusInTileLengths; j++){

                    int destX = mouseGridX + j;
                    int destY = mouseGridY + i;

                    if(destX >= nX - 1 || destY >= nY - 1){
                        continue;
                    }
                    
                    float lambda = strength * kernel[i + maxKernelRadiusInTileLengths][j + maxKernelRadiusInTileLengths];
                    lambda = clamp(lambda, 0, 1);

                    grid[destY][destX] = (1 - lambda) * grid[destY][destX] + lambda * target;

                    gridHasChanged = true;
                }
            }
        }

        window.clear(sf::Color::White);

        if(displayMode == GRID){
            
            float radius = 4;

            sf::CircleShape dot(radius);
            dot.setOrigin(sf::Vector2f(radius, radius));
            
            for(int y = 0; y < nY; y++){
                for(int x = 0; x < nX; x++){

                    sf::Color fillColor = sf::Color((1. - grid[y][x]) * 255., (1. - grid[y][x]) * 255., (1. - grid[y][x]) * 255., 255);
                    dot.setFillColor(fillColor);
                    
                    // each grid value is defined in the middle of the cell
                    dot.setPosition(sf::Vector2f((x + 0.5) * TILE_LENGTH, (y + 0.5) * TILE_LENGTH));
                    window.draw(dot);
                }
            }
        }
        else if(displayMode == CONTOUR || displayMode == INTERPOLATING_CONTOUR){

            if(gridHasChanged){
                
                // reset everything
                cellBeginsAtIndex.clear();
                cellHasSides.clear();
                sides.clear();

                int sideCount = 0;
                
                for(int y = 0; y < nY; y++){
                    for(int x = 0; x < nX; x++){

                        if(x == nX-1 || y == nY - 1){
                            cellHasSides.push_back(0);
                            cellBeginsAtIndex.push_back(sideCount);
                            //sideCount += 0;
                            //sides.insert(sides.end(), toAdd.begin(), toAdd.end());
                            continue;
                        }
                        
                        int bottomLeft = grid[y + 1][x] >= threshold ? 1 : 0;
                        int bottomRight = grid[y + 1][x + 1] >= threshold ? 1 : 0;
                        int topRight = grid[y][x + 1] >= threshold ? 1 : 0;
                        int topLeft = grid[y][x] >= threshold ? 1 : 0;

                        // Based on: https://en.wikipedia.org/wiki/Marching_squares

                        int caseNum = bottomLeft + 2 * bottomRight + 4 * topRight + 8 * topLeft;

                        std::vector<int> toAdd;

                        switch(caseNum){
                            case 0:
                                toAdd = {};
                                break;
                            case 1:
                                toAdd = {2, 3};
                                break;
                            case 2:
                                toAdd = {1, 2};
                                break;
                            case 3:
                                toAdd = {1, 3};
                                break;
                            case 4:
                                toAdd = {0, 1};
                                break;
                            case 5:
                                toAdd = {0, 3, 1, 2};
                                break;
                            case 6:
                                toAdd = {0, 2};
                                break;
                            case 7:
                                toAdd = {0, 3};
                                break;
                            case 8:
                                toAdd = {0, 3};
                                break;
                            case 9:
                                toAdd = {0, 2};
                                break;
                            case 10:
                                toAdd = {0, 1, 2, 3};
                                break;
                            case 11:
                                toAdd = {0, 1};
                                break;
                            case 12:
                                toAdd = {1, 3};
                                break;
                            case 13:
                                toAdd = {1, 2};
                                break;
                            case 14:
                                toAdd = {2, 3};
                                break;
                            case 15:
                                toAdd = {};
                                break;
                            default:
                                printf("ERROR: Unknown case");
                                exit(-1);
                        }

                        int nSides = toAdd.size();

                        cellHasSides.push_back(nSides);
                        cellBeginsAtIndex.push_back(sideCount);

                        sideCount += nSides;

                        sides.insert(sides.end(), toAdd.begin(), toAdd.end());
                    }
                }

                gridHasChanged = false;
            }

            sf::VertexArray line(sf::Lines, 2);
            line[0].color = sf::Color::Black;
            line[1].color = sf::Color::Black;
            
            // code just displays the contour
            for(int y = 0; y < nY - 1; y++){
                for(int x = 0; x < nX - 1; x++){

                    int idx = y * nX + x;

                    int beginIdx = cellBeginsAtIndex[idx];
                    int nSides = cellHasSides[idx];

                    sf::Vector2f tilePos((x + 0.5) * TILE_LENGTH, (y + 0.5) * TILE_LENGTH);

                    // sides are always multiples of two
                    for(int i = 0; i < nSides; i+=2){

                        int startSide = sides[beginIdx + i];
                        int endSide = sides[beginIdx + i + 1];

                        if(displayMode == CONTOUR){
                            line[0].position = tilePos + getRelPosSide(startSide);
                            line[1].position = tilePos + getRelPosSide(endSide);
                        }
                        else{
                            // must be interpolating contour
                            line[0].position = tilePos + getRelPosSide(startSide, (float*)grid, x, y, nX, threshold);
                            line[1].position = tilePos + getRelPosSide(endSide, (float*)grid, x, y, nX, threshold);
                        }
                        
                        window.draw(line);
                    }
                }
            }
        }

        // draw cursor
        sf::CircleShape cursor(kernelRadius);
        cursor.setOrigin(2 * kernelRadius, 2 * kernelRadius);
        cursor.setPosition(sf::Vector2f(mousePos));
        cursor.setFillColor(sf::Color::Transparent);
        cursor.setOutlineColor(sf::Color::Black);
        cursor.setOutlineThickness(2);

        window.draw(cursor);

        drawDisplayMode(window, displayMode, threshold);
        
        window.display();
    }

    return 0;
}