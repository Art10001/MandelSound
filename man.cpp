#include <SDL2/SDL.h>
#include <complex>
#include <cmath>
#include <vector>
#include <iostream>

// Constants for the window and rendering
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
const int MAX_ITERATIONS = 1000;

// Audio settings
const int SAMPLE_RATE = 44100;
const int AUDIO_CHANNELS = 1;  // Mono
const int AUDIO_BUFFER_SIZE = 4096;

// Complex plane boundaries
double xMin = -2.5;
double xMax = 1.0;
double yMin = -1.5;
double yMax = 1.5;

// RGBA helper function
Uint32 createRGBA(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    return ((r << 0) | (g << 8) | (b << 16) | (a << 24));
}

// Calculate the number of iterations for a point in the complex plane
int calculateMandelbrot(double real, double imag) {
    std::complex<double> c(real, imag);
    std::complex<double> z(0, 0);
    
    int iteration = 0;
    while (std::abs(z) < 2.0 && iteration < MAX_ITERATIONS) {
        z = z * z + c;
        iteration++;
    }
    
    return iteration;
}

// Map a value from one range to another
double mapValue(double value, double inMin, double inMax, double outMin, double outMax) {
    return outMin + (outMax - outMin) * ((value - inMin) / (inMax - inMin));
}

// Create a musical sound based on Mandelbrot properties
std::vector<Sint16> createMandelbrotSound(int iterations, double real, double imag) {
    // Base duration and primary frequency
    double duration = 1.5;  // 1.5 seconds
    double primaryFreq;
    
    if (iterations >= MAX_ITERATIONS) {
        // Inside the set - low frequency
        primaryFreq = 110.0;  // A2
    } else {
        // Outside the set - map iterations to frequency
        primaryFreq = mapValue(iterations, 0, MAX_ITERATIONS, 220.0, 880.0);
    }
    
    // Secondary frequencies based on the position
    double secondaryFreq1 = primaryFreq * (1.0 + real * 0.1);
    double secondaryFreq2 = primaryFreq * (1.0 + imag * 0.1);
    double harmonicFreq = primaryFreq * 1.5;  // Perfect fifth
    
    // Generate the complex tone
    int sampleCount = static_cast<int>(SAMPLE_RATE * duration);
    std::vector<Sint16> buffer(sampleCount);
    
    for (int i = 0; i < sampleCount; i++) {
        double time = static_cast<double>(i) / SAMPLE_RATE;
        
        // Create an amplitude envelope (ADSR: Attack, Decay, Sustain, Release)
        double envelope;
        double attackTime = 0.1;
        double decayTime = 0.2;
        double sustainLevel = 0.7;
        double releaseTime = 0.5;
        
        if (time < attackTime) {
            // Attack phase
            envelope = time / attackTime;
        } else if (time < attackTime + decayTime) {
            // Decay phase
            envelope = 1.0 - (1.0 - sustainLevel) * ((time - attackTime) / decayTime);
        } else if (time < duration - releaseTime) {
            // Sustain phase
            envelope = sustainLevel;
        } else {
            // Release phase
            envelope = sustainLevel * (1.0 - (time - (duration - releaseTime)) / releaseTime);
        }
        
        // Mix multiple frequencies with different weights
        double sample = 0.5 * sin(2.0 * M_PI * primaryFreq * time);
        sample += 0.25 * sin(2.0 * M_PI * secondaryFreq1 * time);
        sample += 0.15 * sin(2.0 * M_PI * secondaryFreq2 * time);
        sample += 0.1 * sin(2.0 * M_PI * harmonicFreq * time);
        
        // Add a slight vibrato effect
        double vibratoFreq = 6.0;  // 6 Hz vibrato
        double vibratoAmount = 0.01;
        double vibrato = sin(2.0 * M_PI * vibratoFreq * time) * vibratoAmount;
        sample += vibrato * sin(2.0 * M_PI * primaryFreq * (1.0 + vibrato) * time);
        
        // Apply the envelope
        sample *= envelope;
        
        // Convert to 16-bit signed
        buffer[i] = static_cast<Sint16>(sample * 32767);
    }
    
    return buffer;
}

void renderMandelbrot(SDL_Renderer* renderer, SDL_Texture* texture) {
    // Create a buffer for pixel data
    Uint32* pixels = new Uint32[SCREEN_WIDTH * SCREEN_HEIGHT];
    
    // Render the Mandelbrot set
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            double real = mapValue(x, 0, SCREEN_WIDTH, xMin, xMax);
            double imag = mapValue(y, 0, SCREEN_HEIGHT, yMin, yMax);
            
            int iterations = calculateMandelbrot(real, imag);
            
            Uint8 r, g, b;
            if (iterations == MAX_ITERATIONS) {
                // Inside the set - black
                r = g = b = 0;
            } else {
                // Outside the set - color based on iterations
                double hue = mapValue(iterations % 64, 0, 64, 0, 1);
                
                // HSV to RGB conversion
                double h = hue * 6.0;
                int i = static_cast<int>(h);
                double f = h - i;
                double q = 1.0 - f;
                
                switch (i % 6) {
                    case 0: r = 255; g = static_cast<Uint8>(f * 255); b = 0; break;
                    case 1: r = static_cast<Uint8>(q * 255); g = 255; b = 0; break;
                    case 2: r = 0; g = 255; b = static_cast<Uint8>(f * 255); break;
                    case 3: r = 0; g = static_cast<Uint8>(q * 255); b = 255; break;
                    case 4: r = static_cast<Uint8>(f * 255); g = 0; b = 255; break;
                    case 5: r = 255; g = 0; b = static_cast<Uint8>(q * 255); break;
                }
            }
            
            pixels[y * SCREEN_WIDTH + x] = createRGBA(r, g, b, 255);
        }
    }
    
    // Update the texture with the rendered Mandelbrot set
    SDL_UpdateTexture(texture, NULL, pixels, SCREEN_WIDTH * sizeof(Uint32));
    
    // Render the texture to the screen
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    
    // Free the pixel buffer
    delete[] pixels;
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
    
    // Create renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
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
    want.format = AUDIO_S16SYS;  // 16-bit signed audio
    want.channels = AUDIO_CHANNELS;
    want.samples = AUDIO_BUFFER_SIZE;
    want.callback = NULL;  // Using SDL_QueueAudio
    
    SDL_AudioDeviceID audioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audioDevice == 0) {
        std::cerr << "Failed to open audio: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Create a texture for storing the Mandelbrot set
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, 
                                            SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // Render the initial Mandelbrot set
    renderMandelbrot(renderer, texture);
    
    // Main loop flag
    bool quit = false;
    
    // Event handler
    SDL_Event e;
    
    // While application is running
    while (!quit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // User requests quit
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            // Mouse click
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mouseX = e.button.x;
                int mouseY = e.button.y;
                
                // Convert screen coordinates to complex plane
                double real = mapValue(mouseX, 0, SCREEN_WIDTH, xMin, xMax);
                double imag = mapValue(mouseY, 0, SCREEN_HEIGHT, yMin, yMax);
                
                // Calculate iterations at clicked point
                int iterations = calculateMandelbrot(real, imag);
                
                // Create and play a sound based on the point properties
                std::vector<Sint16> soundBuffer = createMandelbrotSound(iterations, real, imag);
                
                // Stop any currently playing audio
                SDL_ClearQueuedAudio(audioDevice);
                
                // Queue the new audio
                SDL_QueueAudio(audioDevice, soundBuffer.data(), soundBuffer.size() * sizeof(Sint16));
                
                // Start playing
                SDL_PauseAudioDevice(audioDevice, 0);
                
                std::cout << "Clicked at (" << real << ", " << imag << ") with " 
                          << iterations << " iterations." << std::endl;
            }
            // Zoom with mouse wheel
            else if (e.type == SDL_MOUSEWHEEL) {
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);
                
                // Convert screen coordinates to complex plane
                double centerReal = mapValue(mouseX, 0, SCREEN_WIDTH, xMin, xMax);
                double centerImag = mapValue(mouseY, 0, SCREEN_HEIGHT, yMin, yMax);
                
                // Zoom factor
                double zoomFactor = (e.wheel.y > 0) ? 0.5 : 2.0;
                
                // Calculate new boundaries while keeping center fixed
                double newWidth = (xMax - xMin) * zoomFactor;
                double newHeight = (yMax - yMin) * zoomFactor;
                
                xMin = centerReal - newWidth / 2;
                xMax = centerReal + newWidth / 2;
                yMin = centerImag - newHeight / 2;
                yMax = centerImag + newHeight / 2;
                
                // Re-render the Mandelbrot set with new boundaries
                renderMandelbrot(renderer, texture);
            }
        }
    }
    
    // Free resources and close SDL
    SDL_DestroyTexture(texture);
    SDL_CloseAudioDevice(audioDevice);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
