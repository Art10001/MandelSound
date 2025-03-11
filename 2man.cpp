#include <SDL2/SDL.h>
#include <complex>
#include <cmath>
#include <vector>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>

// Constants for the window and rendering
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
int MAX_ITERATIONS = 100; // Adaptive maximum iterations

// Multithreading settings
const int NUM_THREADS = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4;

// Audio settings
const int SAMPLE_RATE = 44100;
const int AUDIO_CHANNELS = 1;
const int AUDIO_BUFFER_SIZE = 2048;

// Complex plane boundaries
double xMin = -2.5;
double xMax = 1.0;
double yMin = -1.5;
double yMax = 1.5;

// Previous rendering boundaries for optimization
double prev_xMin = 0.0;
double prev_xMax = 0.0;
double prev_yMin = 0.0;
double prev_yMax = 0.0;

// Precision control for dynamic detail
std::atomic<bool> needsUpdate(true);
std::atomic<bool> isHighQuality(false);
std::atomic<bool> isRenderingHighQuality(false);

// Calculate the number of iterations for a point in the complex plane
inline int calculateMandelbrot(double real, double imag, int maxIter) {
    double x = 0;
    double y = 0;
    double x2 = 0;
    double y2 = 0;
    
    int iteration = 0;
    // Using the optimized algorithm avoiding complex numbers and sqrt
    while (x2 + y2 < 4.0 && iteration < maxIter) {
        y = 2 * x * y + imag;
        x = x2 - y2 + real;
        x2 = x * x;
        y2 = y * y;
        iteration++;
    }
    
    return iteration;
}

// Map a value from one range to another
inline double mapValue(double value, double inMin, double inMax, double outMin, double outMax) {
    return outMin + (outMax - outMin) * ((value - inMin) / (inMax - inMin));
}

// Create a musical sound based on Mandelbrot properties
std::vector<Sint16> createMandelbrotSound(int iterations, double real, double imag) {
    // Base duration and primary frequency
    double duration = 1.0;  // Reduced to 1 second for better responsiveness
    double primaryFreq;
    
    if (iterations >= MAX_ITERATIONS) {
        primaryFreq = 110.0;  // A2
    } else {
        primaryFreq = mapValue(iterations, 0, MAX_ITERATIONS, 220.0, 880.0);
    }
    
    double secondaryFreq1 = primaryFreq * (1.0 + real * 0.1);
    double secondaryFreq2 = primaryFreq * (1.0 + imag * 0.1);
    double harmonicFreq = primaryFreq * 1.5;
    
    int sampleCount = static_cast<int>(SAMPLE_RATE * duration);
    std::vector<Sint16> buffer(sampleCount);
    
    for (int i = 0; i < sampleCount; i++) {
        double time = static_cast<double>(i) / SAMPLE_RATE;
        double envelope;
        double attackTime = 0.05;  // Shorter attack
        double decayTime = 0.1;    // Shorter decay
        double sustainLevel = 0.7;
        double releaseTime = 0.3;  // Shorter release
        
        if (time < attackTime) {
            envelope = time / attackTime;
        } else if (time < attackTime + decayTime) {
            envelope = 1.0 - (1.0 - sustainLevel) * ((time - attackTime) / decayTime);
        } else if (time < duration - releaseTime) {
            envelope = sustainLevel;
        } else {
            envelope = sustainLevel * (1.0 - (time - (duration - releaseTime)) / releaseTime);
        }
        
        double sample = 0.5 * sin(2.0 * M_PI * primaryFreq * time);
        sample += 0.25 * sin(2.0 * M_PI * secondaryFreq1 * time);
        sample += 0.15 * sin(2.0 * M_PI * secondaryFreq2 * time);
        sample += 0.1 * sin(2.0 * M_PI * harmonicFreq * time);
        
        sample *= envelope;
        buffer[i] = static_cast<Sint16>(sample * 32767);
    }
    
    return buffer;
}

// Thread function to render a portion of the Mandelbrot set
void renderMandelbrotSection(Uint32* pixels, int startY, int endY, int width, int height, 
                          double xMin, double xMax, double yMin, double yMax, int maxIterations) {
    for (int y = startY; y < endY; y++) {
        for (int x = 0; x < width; x++) {
            double real = mapValue(x, 0, width, xMin, xMax);
            double imag = mapValue(y, 0, height, yMin, yMax);
            
            int iterations = calculateMandelbrot(real, imag, maxIterations);
            
            Uint8 r, g, b;
            if (iterations == maxIterations) {
                // Inside the set - black
                r = g = b = 0;
            } else {
                // Outside the set - color based on iterations
                double hue = mapValue(iterations % 64, 0, 64, 0, 1);
                double saturation = 0.8;
                double value = iterations < maxIterations ? 1.0 : 0.0;
                
                // HSV to RGB conversion
                double h = hue * 6.0;
                int i = static_cast<int>(h);
                double f = h - i;
                double p = value * (1.0 - saturation);
                double q = value * (1.0 - saturation * f);
                double t = value * (1.0 - saturation * (1.0 - f));
                
                switch (i % 6) {
                    case 0: r = value * 255; g = t * 255; b = p * 255; break;
                    case 1: r = q * 255; g = value * 255; b = p * 255; break;
                    case 2: r = p * 255; g = value * 255; b = t * 255; break;
                    case 3: r = p * 255; g = q * 255; b = value * 255; break;
                    case 4: r = t * 255; g = p * 255; b = value * 255; break;
                    case 5: r = value * 255; g = p * 255; b = q * 255; break;
                }
            }
            
            pixels[y * width + x] = (r) | (g << 8) | (b << 16) | (255 << 24);
        }
    }
}

void renderMandelbrot(SDL_Renderer* renderer, SDL_Texture* texture, bool highQuality = true) {
    Uint32* pixels = new Uint32[SCREEN_WIDTH * SCREEN_HEIGHT];
    
    // Skip rendering if boundaries haven't changed and high-quality is already done
    if (highQuality && !needsUpdate && 
        prev_xMin == xMin && prev_xMax == xMax && 
        prev_yMin == yMin && prev_yMax == yMax) {
        delete[] pixels;
        return;
    }
    
    // Store current boundaries
    prev_xMin = xMin;
    prev_xMax = xMax;
    prev_yMin = yMin;
    prev_yMax = yMax;
    
    // Calculate appropriate iterations based on zoom level
    int localMaxIterations = highQuality ? MAX_ITERATIONS : MAX_ITERATIONS / 4;
    
    // Use multithreading for better performance
    std::vector<std::thread> threads;
    int sectionHeight = SCREEN_HEIGHT / NUM_THREADS;
    
    // Create the worker threads
    for (int i = 0; i < NUM_THREADS; i++) {
        int startY = i * sectionHeight;
        int endY = (i == NUM_THREADS - 1) ? SCREEN_HEIGHT : startY + sectionHeight;
        
        threads.push_back(std::thread(
            renderMandelbrotSection, 
            pixels, startY, endY, SCREEN_WIDTH, SCREEN_HEIGHT, 
            xMin, xMax, yMin, yMax, localMaxIterations
        ));
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Update the texture with the rendered Mandelbrot set
    SDL_UpdateTexture(texture, NULL, pixels, SCREEN_WIDTH * sizeof(Uint32));
    
    // Render the texture to the screen
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    
    delete[] pixels;
    
    // Mark as updated
    if (highQuality) {
        needsUpdate = false;
        isHighQuality = true;
        isRenderingHighQuality = false;
    }
}

// Dynamic iteration adjustment based on zoom level
void updateIterations() {
    // Calculate the zoom level
    double initialRange = 3.5; // Original width of view
    double currentRange = xMax - xMin;
    double zoomLevel = initialRange / currentRange;
    
    // Adjust iterations based on zoom level, with a minimum and maximum
    MAX_ITERATIONS = static_cast<int>(100 * sqrt(zoomLevel));
    if (MAX_ITERATIONS < 100) MAX_ITERATIONS = 100;
    if (MAX_ITERATIONS > 2000) MAX_ITERATIONS = 2000;
}

int main(int argc, char* args[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Create window
    SDL_Window* window = SDL_CreateWindow("Mandelbrot Set with Sound", SDL_WINDOWPOS_UNDEFINED, 
                                        SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Create renderer with hardware acceleration
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Set up audio
    SDL_AudioSpec want, have;
    SDL_memset(&want, 0, sizeof(want));
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = AUDIO_CHANNELS;
    want.samples = AUDIO_BUFFER_SIZE;
    want.callback = NULL;
    
    SDL_AudioDeviceID audioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audioDevice == 0) {
        std::cerr << "Failed to open audio: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Create a texture for the Mandelbrot set
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, 
                                          SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // Render the initial Mandelbrot set (low quality first for responsiveness)
    renderMandelbrot(renderer, texture, false);
    
    // Main loop
    bool quit = false;
    SDL_Event e;
    Uint32 lastRenderTime = 0;
    const Uint32 RENDER_DELAY = 50; // 50ms between low and high quality renders
    
    while (!quit) {
        // Handle events
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    int mouseX = e.button.x;
                    int mouseY = e.button.y;
                    
                    double real = mapValue(mouseX, 0, SCREEN_WIDTH, xMin, xMax);
                    double imag = mapValue(mouseY, 0, SCREEN_HEIGHT, yMin, yMax);
                    
                    // Calculate iterations at clicked point
                    int iterations = calculateMandelbrot(real, imag, MAX_ITERATIONS);
                    
                    // Create and play sound
                    std::vector<Sint16> soundBuffer = createMandelbrotSound(iterations, real, imag);
                    SDL_ClearQueuedAudio(audioDevice);
                    SDL_QueueAudio(audioDevice, soundBuffer.data(), soundBuffer.size() * sizeof(Sint16));
                    SDL_PauseAudioDevice(audioDevice, 0);
                    
                    std::cout << "Clicked at (" << real << ", " << imag << ") with " 
                              << iterations << " iterations." << std::endl;
                }
            }
            else if (e.type == SDL_MOUSEWHEEL) {
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);
                
                // Convert screen coordinates to complex plane
                double centerReal = mapValue(mouseX, 0, SCREEN_WIDTH, xMin, xMax);
                double centerImag = mapValue(mouseY, 0, SCREEN_HEIGHT, yMin, yMax);
                
                // Zoom factor - make it smoother
                double zoomFactor = (e.wheel.y > 0) ? 0.8 : 1.25;
                
                // Calculate new boundaries
                double newWidth = (xMax - xMin) * zoomFactor;
                double newHeight = (yMax - yMin) * zoomFactor;
                
                xMin = centerReal - newWidth / 2;
                xMax = centerReal + newWidth / 2;
                yMin = centerImag - newHeight / 2;
                yMax = centerImag + newHeight / 2;
                
                // Update iterations based on zoom level
                updateIterations();
                
                // Mark for re-rendering
                needsUpdate = true;
                isHighQuality = false;
                
                // Render at low quality immediately for responsiveness
                renderMandelbrot(renderer, texture, false);
                lastRenderTime = SDL_GetTicks();
            }
        }
        
        // Two-phase rendering strategy: quick render first, then high quality
        Uint32 currentTime = SDL_GetTicks();
        if (needsUpdate && !isHighQuality && !isRenderingHighQuality && 
            (currentTime - lastRenderTime > RENDER_DELAY)) {
            isRenderingHighQuality = true;
            renderMandelbrot(renderer, texture, true);
        }
        
        // Small delay to prevent hogging the CPU
        SDL_Delay(1);
    }
    
    // Clean up
    SDL_DestroyTexture(texture);
    SDL_CloseAudioDevice(audioDevice);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
