/* Transcribed from Munt's mt32emu/src/Enumerations.h - see munt/README.md. */
#pragma once

namespace MT32Emu {

enum RendererType {
    /** 16-bit signed samples, accurate wave generator model. */
    RendererType_BIT16S,
    /** Float samples, simplified wave generator model. */
    RendererType_FLOAT
};

}
