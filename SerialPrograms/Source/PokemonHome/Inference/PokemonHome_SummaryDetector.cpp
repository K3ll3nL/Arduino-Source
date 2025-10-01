#include "PokemonHome_SummaryDetector.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"


namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{



SummaryDetector::~SummaryDetector() = default;

SummaryDetector::SummaryDetector(Color color)
    : m_color(color)
{}

void SummaryDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool SummaryDetector::detect(const ImageViewRGB32& screen){
    return false;
}



SummaryWatcher::~SummaryWatcher() = default;

SummaryWatcher::SummaryWatcher(Color color)
    : VisualInferenceCallback("SummaryWatcher")
    , m_detector(color)
{}

void SummaryWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);
}

bool SummaryWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){

    return m_detector.detect(screen);
}



}
}
}
