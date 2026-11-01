#include "Inserts.h"

#include "../FxChainJson.h"
#include "../JsonSupport.h"
#include "../RequestDispatch.h"
#include "../TestSupport.h"

namespace ApiTest::InstrumentPageApi
{

void runInserts (SessionRig& rig)
{
    auto& client = rig.client();

    auto response = client.Get ("/patches/0/fx");
    check (response && response->status == 200, "GET /patches/0/fx returns 200");
    check (insertCount (parseJson (response->body)) == 0,
           "the default patch starts with an empty chain");

    response = postWithDispatch (client, "/patches/0/fx",
                                 "{\"type\":\"compressor\",\"position\":0,"
                                 "\"parameters\":{\"thresholdDb\":-30,\"ratio\":4}}");
    check (response && response->status == 200, "POST /patches/0/fx returns 200");

    auto chain = response ? parseJson (response->body) : juce::var {};

    check (insertCount (chain) == 1, "the chain editor appended the compressor");
    check (insertType (chain, 0) == "compressor", "the first insert is the compressor");
    check (chainLatency (chain) > 0, "the insert reports its latency contribution");

    const int compressorLatency = chainLatency (chain);

    response = patchWithDispatch (client, "/patches/0/fx/0", "{\"bypassed\":true}");
    check (response && response->status == 200, "PATCH /patches/0/fx/0 returns 200");

    chain = parseJson (response->body);

    check (insertCount (chain) == 1
               && static_cast<bool> (property ((*arrayProperty (chain, "inserts"))[0],
                                               "bypassed")),
           "the compressor is bypassed");
    check (chainLatency (chain) == 0,
           "a bypassed insert leaves the latency budget");
    check (compressorLatency > 0, "the compressor reported latency before it was bypassed");

    response = postWithDispatch (client, "/patches/0/fx", "{\"type\":\"delay\"}");
    check (response && response->status == 200, "a delay insert is accepted");
    check (insertCount (parseJson (response->body)) == 2, "the chain now has two inserts");

    response = postWithDispatch (client, "/patches/0/fx/reorder", "{\"order\":[1,0]}");
    check (response && response->status == 200, "POST /fx/reorder returns 200");

    chain = parseJson (response->body);

    check (insertType (chain, 0) == "delay" && insertType (chain, 1) == "compressor",
           "the reorder reached the engine");

    response = deleteWithDispatch (client, "/patches/0/fx/0");
    check (response && response->status == 200, "DELETE /patches/0/fx/0 returns 200");
    check (insertCount (parseJson (response->body)) == 1, "the chain editor removed one insert");

    response = putWithDispatch (client, "/patches/0/fx",
                                "{\"chain\":[{\"type\":\"eq\"},{\"type\":\"reverb\",\"wet\":0.4}]}");
    check (response && response->status == 200, "PUT /fx replaces the whole chain");
    check (insertCount (parseJson (response->body)) == 2, "the replacement chain is complete");

    response = patchWithDispatch (client, "/patches/0/fx/9", "{\"bypassed\":true}");
    check (response && response->status == 404, "an out-of-range insert is rejected");

    response = postWithDispatch (client, "/patches/0/fx", "{\"type\":\"chorus\"}");
    check (response && response->status == 400, "an unknown insert type is rejected");
}

}
