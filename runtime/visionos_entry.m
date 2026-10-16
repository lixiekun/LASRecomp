// SDL owns the UIKit scene and calls the existing game entry on the main thread.
#import <Foundation/Foundation.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdlib.h>
extern int las_game_main(int argc, char **argv);
int main(int argc, char **argv) {
    @autoreleasepool {
        NSString *game = [NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"game"];
        NSString *font = [NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"LASFallback-Regular.ttf"];
        if ([NSFileManager.defaultManager fileExistsAtPath:font]) setenv("LARECOMP_FONT", font.fileSystemRepresentation, 1);
        char *args[] = {argv[0], (char *)game.fileSystemRepresentation, "--play", NULL};
        return las_game_main(3, args);
    }
}
