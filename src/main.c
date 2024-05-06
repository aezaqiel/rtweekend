#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <cglm/cglm.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

int main(void)
{
    const uint32_t image_width = 1280;
    const uint32_t image_height = 720;
    const char* image_filepath = "image.png";

    uint8_t* image_buffer = (uint8_t*)calloc(image_width * image_height * 3, sizeof(uint8_t));

    for (uint32_t j = 0; j < image_height; ++j) {
        for (uint32_t i = 0; i < image_width; ++i) {
            float u = (float)(i) / (float)(image_width - 1);
            float v = (float)(j) / (float)(image_height - 1);

            vec3 pixel = { u, v, 0.25f };

            uint8_t r = (uint8_t)(pixel[0] * 255.0f);
            uint8_t g = (uint8_t)(pixel[1] * 255.0f);
            uint8_t b = (uint8_t)(pixel[2] * 255.0f);

            uint32_t index = (i + j * image_width) * 3;

            image_buffer[index + 0] = r;
            image_buffer[index + 1] = g;
            image_buffer[index + 2] = b;
        }
    }

    stbi_flip_vertically_on_write(true);

    if (stbi_write_png(image_filepath, image_width, image_height, 3, image_buffer, image_width * 3) == 0) {
        fprintf(stderr, "failed to save image to %s\n", image_filepath);
        return EXIT_FAILURE;
    }

    printf("image saved to %s\n", image_filepath);

    return EXIT_SUCCESS;
}
