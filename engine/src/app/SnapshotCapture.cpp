#include "SnapshotCapture.h"

#include <juce_graphics/juce_graphics.h>

#include <set>

#if JUCE_LINUX
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
#endif

namespace ihub::app
{

int countDistinctColours (const juce::Image& image)
{
    if (image.isNull() || image.getWidth() <= 0 || image.getHeight() <= 0)
        return 0;

    std::set<juce::uint32> colours;
    constexpr int steps = 64;

    for (int y = 0; y < steps; ++y)
    {
        const int py = juce::jlimit (0, image.getHeight() - 1,
                                     y * image.getHeight() / steps);

        for (int x = 0; x < steps; ++x)
        {
            const int px = juce::jlimit (0, image.getWidth() - 1,
                                         x * image.getWidth() / steps);
            colours.insert (image.getPixelAt (px, py).getARGB());
        }
    }

    return static_cast<int> (colours.size());
}

int countBrightPixels (const juce::Image& image)
{
    if (image.isNull() || image.getWidth() <= 0 || image.getHeight() <= 0)
        return 0;

    int bright = 0;
    constexpr int steps = 64;

    for (int y = 0; y < steps; ++y)
    {
        const int py = juce::jlimit (0, image.getHeight() - 1,
                                     y * image.getHeight() / steps);

        for (int x = 0; x < steps; ++x)
        {
            const int px = juce::jlimit (0, image.getWidth() - 1,
                                         x * image.getWidth() / steps);

            if (image.getPixelAt (px, py).getPerceivedBrightness() > 0.55f)
                ++bright;
        }
    }

    return bright;
}

bool looksRendered (const juce::Image& image)
{
    return countDistinctColours (image) >= 8
           && countBrightPixels (image) >= 16;
}

#if JUCE_LINUX
namespace
{
juce::uint8 maskToByte (unsigned long pixel, unsigned long mask)
{
    if (mask == 0)
        return 0;

    int shift = 0;

    while (((mask >> shift) & 1UL) == 0)
        ++shift;

    const auto max = mask >> shift;
    const auto value = (pixel & mask) >> shift;

    return static_cast<juce::uint8> ((value * 255 + max / 2) / max);
}

juce::Image captureNativeWindow (juce::Component& component)
{
    auto* peer = component.getPeer();

    if (peer == nullptr)
        return {};

    auto* display = XOpenDisplay (nullptr);

    if (display == nullptr)
        return {};

    const auto window = static_cast<Window> (reinterpret_cast<juce::pointer_sized_uint> (peer->getNativeHandle()));
    XWindowAttributes attributes{};

    if (XGetWindowAttributes (display, window, &attributes) == 0
        || attributes.width <= 0 || attributes.height <= 0)
    {
        XCloseDisplay (display);
        return {};
    }

    XRaiseWindow (display, window);
    XSync (display, False);

    Window child = 0;
    int rootX = 0;
    int rootY = 0;
    XTranslateCoordinates (display, window, DefaultRootWindow (display),
                           0, 0, &rootX, &rootY, &child);

    const auto width = static_cast<unsigned int> (attributes.width);
    const auto height = static_cast<unsigned int> (attributes.height);

    XImage* source = XGetImage (display, window, 0, 0, width, height,
                                AllPlanes, ZPixmap);

    if (source == nullptr)
        source = XGetImage (display, DefaultRootWindow (display),
                            rootX, rootY, width, height, AllPlanes, ZPixmap);

    if (source == nullptr)
    {
        XCloseDisplay (display);
        return {};
    }

    juce::Image image (juce::Image::ARGB, attributes.width, attributes.height, true);

    for (int y = 0; y < attributes.height; ++y)
    {
        for (int x = 0; x < attributes.width; ++x)
        {
            const auto pixel = XGetPixel (source, x, y);
            const auto red = maskToByte (pixel, source->red_mask);
            const auto green = maskToByte (pixel, source->green_mask);
            const auto blue = maskToByte (pixel, source->blue_mask);

            image.setPixelAt (x, y, juce::Colour (red, green, blue));
        }
    }

    XDestroyImage (source);
    XCloseDisplay (display);

    return image;
}
}
#else
namespace
{
juce::Image captureNativeWindow (juce::Component&)
{
    return {};
}
}
#endif

juce::Image captureSnapshot (juce::Component& component)
{
    auto nativeImage = captureNativeWindow (component);

    if (looksRendered (nativeImage))
        return nativeImage;

    auto componentImage = component.createComponentSnapshot (component.getLocalBounds());

    if (looksRendered (componentImage))
        return componentImage;

    return nativeImage.isNull() ? componentImage : nativeImage;
}

bool writeSnapshot (const juce::Image& image, const juce::File& file)
{
    file.getParentDirectory().createDirectory();

    auto stream = file.createOutputStream();

    if (stream == nullptr || ! stream->openedOk())
        return false;

    juce::PNGImageFormat png;
    return png.writeImageToStream (image, *stream);
}

}
