#include "Fixtures.h"

namespace AudioTest::Transport
{

void advance (AudioEngine::Transport& transport, std::int64_t frames, int blockSize)
{
    while (frames > 0)
    {
        const auto block = static_cast<int> (
            std::min<std::int64_t> (frames, static_cast<std::int64_t> (blockSize)));

        transport.process (block);
        frames -= block;
    }
}

}
