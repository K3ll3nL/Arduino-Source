#ifndef PokemonAutomation_PokemonHome_SummaryDetector_H
#define PokemonAutomation_PokemonHome_SummaryDetector_H


#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"
namespace PokemonAutomation{

class VideoOverlaySet;
class VideoOverlay;
class OverlayBoxScope;

namespace NintendoSwitch{
namespace PokemonHome{


class SummaryDetector : public StaticScreenDetector{
public:
    SummaryDetector(Color color);
    virtual ~SummaryDetector();

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

protected:
    Color m_color;
    ImageFloatBox m_box;
};

class SummaryWatcher : public VisualInferenceCallback{
public:
    SummaryWatcher(Color color);
    virtual ~SummaryWatcher();

    //PokemonData get_pokemon();

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool process_frame(const ImageViewRGB32& frame, WallClock timestamp) override;


protected:
    SummaryDetector m_detector;
    //PokemonData m_pokemon
};


}
}
}
#endif // POKEMONHOME_SUMMARYDETECTOR_H
