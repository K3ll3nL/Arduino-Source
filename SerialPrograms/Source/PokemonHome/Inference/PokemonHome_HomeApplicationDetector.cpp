/*  Object Name Detector
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/ImageMatch/WaterfillTemplateMatcher.h"
#include "CommonTools/Images/WaterfillUtilities.h"
#include "CommonTools/OCR/OCR_RawOCR.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "Kernels/Waterfill/Kernels_Waterfill_Types.h"
#include <iostream>
#include "PokemonHome_HomeApplicationDetector.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{



HomeApplicationDetector::~HomeApplicationDetector() = default;

HomeApplicationDetector::HomeApplicationDetector(Color color)
    : m_color(color)
{}

void HomeApplicationDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool HomeApplicationDetector::detect(const ImageViewRGB32& screen){

    // Title screen banner says "Push any button"
    ImageFloatBox title_screen_box(0.3, 0.8, 0.4, 0.075);
    char chars[] = "\n\r—";
    std::string box_name = OCR::ocr_read(Language::English, extract_box_reference(screen, title_screen_box));
    for(auto a:chars){box_name.erase(std::remove(box_name.begin(),box_name.end(), a),box_name.end());}
    if(box_name == "Push any button"){
        return true;
    }

    // In most main menus, where the minus button for help shows
    ImageFloatBox minus_help_corner(0.03, 0.965, 0.06, 0.03);
    std::string minus_button = OCR::ocr_read(Language::English, extract_box_reference(screen, minus_help_corner));
    for(auto a:chars){minus_button.erase(std::remove(minus_button.begin(),minus_button.end(), a),minus_button.end());}
    ImageFloatBox top_green(0.36, 0.01, 0.001, 0.001);
    ImageFloatBox top_white(0.36, 0.076, 0.001, 0.001);
    FloatPixel green_pixel = image_stats(extract_box_reference(screen, top_green)).average;
    FloatPixel white_pixel = image_stats(extract_box_reference(screen, top_white)).average;
    if(minus_button == "Help" && euclidean_distance(green_pixel, FloatPixel(149, 248, 212))==0 && euclidean_distance(white_pixel, FloatPixel(255, 255, 255))==0){
        return true;
    }

    // No other instances found, did not detect
    return false;
}



HomeApplicationWatcher::~HomeApplicationWatcher() = default;

HomeApplicationWatcher::HomeApplicationWatcher(Color color)
    : VisualInferenceCallback("HomeApplicationWatcher")
    , m_detector(color)
{}

void HomeApplicationWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);
}

bool HomeApplicationWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){

    return m_detector.detect(screen);
}




HomeTitleScreenDetector::~HomeTitleScreenDetector() = default;

HomeTitleScreenDetector::HomeTitleScreenDetector(Color color)
    : m_color(color)
{}

void HomeTitleScreenDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool HomeTitleScreenDetector::detect(const ImageViewRGB32& screen){

    // Title screen banner says "Push any button"
    ImageFloatBox title_screen_box(0.3, 0.8, 0.4, 0.075);
    char chars[] = "\n\r—";
    std::string box_name = OCR::ocr_read(Language::English, extract_box_reference(screen, title_screen_box));
    for(auto a:chars){box_name.erase(std::remove(box_name.begin(),box_name.end(), a),box_name.end());}
    if(box_name == "Push any button"){
        return true;
    }

    // No other instances found, did not detect
    return false;
}



HomeTitleScreenWatcher::~HomeTitleScreenWatcher() = default;

HomeTitleScreenWatcher::HomeTitleScreenWatcher(Color color)
    : VisualInferenceCallback("HomeApplicationWatcher")
    , m_detector(color)
{}

void HomeTitleScreenWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);
}

bool HomeTitleScreenWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){

    return m_detector.detect(screen);
}





HomeMainMenuDetector::~HomeMainMenuDetector() = default;

HomeMainMenuDetector::HomeMainMenuDetector(Color color)
    : m_color(color)
{}

void HomeMainMenuDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool HomeMainMenuDetector::detect(const ImageViewRGB32& screen){

    char chars[] = "\n\r—";

    // In most main menus, where the minus button for help shows
    ImageFloatBox minus_help_corner(0.03, 0.965, 0.06, 0.03);
    ImageFloatBox menu_read_corner(0.06, 0.021, 0.25, 0.05);
    std::string minus_button = OCR::ocr_read(Language::English, extract_box_reference(screen, minus_help_corner));
    std::string menu_text = OCR::ocr_read(Language::English, extract_box_reference(screen, menu_read_corner));
    for(auto a:chars){minus_button.erase(std::remove(minus_button.begin(),minus_button.end(), a),minus_button.end());}
    for(auto a:chars){menu_text.erase(std::remove(menu_text.begin(),menu_text.end(), a),menu_text.end());}
    ImageFloatBox top_green(0.36, 0.01, 0.001, 0.001);
    ImageFloatBox top_white(0.36, 0.076, 0.001, 0.001);
    FloatPixel green_pixel = image_stats(extract_box_reference(screen, top_green)).average;
    FloatPixel white_pixel = image_stats(extract_box_reference(screen, top_white)).average;
    if(menu_text == "MAIN MENU" && minus_button == "Help" && euclidean_distance(green_pixel, FloatPixel(149, 248, 212))==0 && euclidean_distance(white_pixel, FloatPixel(255, 255, 255))==0){
        return true;
    }

    // No other instances found, did not detect
    return false;
}



HomeMainMenuWatcher::~HomeMainMenuWatcher() = default;

HomeMainMenuWatcher::HomeMainMenuWatcher(Color color)
    : VisualInferenceCallback("HomeApplicationWatcher")
    , m_detector(color)
{}

void HomeMainMenuWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);
}

bool HomeMainMenuWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){

    return m_detector.detect(screen);
}



HomeGameSelectDetector::~HomeGameSelectDetector() = default;

HomeGameSelectDetector::HomeGameSelectDetector(Color color)
    : m_color(color)
{}

void HomeGameSelectDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool HomeGameSelectDetector::detect(const ImageViewRGB32& screen){

    char chars[] = "\n\r—.,";

    // In most main menus, where the minus button for help shows
    ImageFloatBox minus_help_corner(0.03, 0.965, 0.06, 0.03);
    ImageFloatBox help_text(0.3, 0.1, 0.41, 0.05);
    std::string minus_button = OCR::ocr_read(Language::English, extract_box_reference(screen, minus_help_corner));
    std::string menu_text = OCR::ocr_read(Language::English, extract_box_reference(screen, help_text));
    for(auto a:chars){minus_button.erase(std::remove(minus_button.begin(),minus_button.end(), a),minus_button.end());}
    for(auto a:chars){menu_text.erase(std::remove(menu_text.begin(),menu_text.end(), a),menu_text.end());}
    ImageFloatBox top_green(0.36, 0.01, 0.001, 0.001);
    ImageFloatBox top_white(0.36, 0.076, 0.001, 0.001);
    FloatPixel green_pixel = image_stats(extract_box_reference(screen, top_green)).average;
    FloatPixel white_pixel = image_stats(extract_box_reference(screen, top_white)).average;
    if(menu_text == "Select a game to connect to Pokémon HOME" && minus_button == "Help" && euclidean_distance(green_pixel, FloatPixel(149, 248, 212))==0 && euclidean_distance(white_pixel, FloatPixel(255, 255, 255))==0){
        return true;
    }

    // No other instances found, did not detect
    return false;
}



HomeGameSelectWatcher::~HomeGameSelectWatcher() = default;

HomeGameSelectWatcher::HomeGameSelectWatcher(Color color)
    : VisualInferenceCallback("HomeApplicationWatcher")
    , m_detector(color)
{}

void HomeGameSelectWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);
}

bool HomeGameSelectWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){

    return m_detector.detect(screen);
}



HomeListViewDetector::~HomeListViewDetector() = default;

HomeListViewDetector::HomeListViewDetector(Color color)
    : m_color(color)
{}

void HomeListViewDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool HomeListViewDetector::detect(const ImageViewRGB32& screen){

    char chars[] = "\n\r—";

    // In most main menus, where the minus button for help shows
    ImageFloatBox minus_help_corner(0.03, 0.965, 0.06, 0.03);
    ImageFloatBox menu_read_corner(0.06, 0.021, 0.25, 0.05);
    std::string minus_button = OCR::ocr_read(Language::English, extract_box_reference(screen, minus_help_corner));
    std::string menu_text = OCR::ocr_read(Language::English, extract_box_reference(screen, menu_read_corner));
    for(auto a:chars){minus_button.erase(std::remove(minus_button.begin(),minus_button.end(), a),minus_button.end());}
    for(auto a:chars){menu_text.erase(std::remove(menu_text.begin(),menu_text.end(), a),menu_text.end());}
    ImageFloatBox top_green(0.36, 0.01, 0.001, 0.001);
    ImageFloatBox top_white(0.36, 0.076, 0.001, 0.001);
    FloatPixel green_pixel = image_stats(extract_box_reference(screen, top_green)).average;
    FloatPixel white_pixel = image_stats(extract_box_reference(screen, top_white)).average;
    if(menu_text == "POKEMON LIST" && minus_button == "Help" && euclidean_distance(green_pixel, FloatPixel(149, 248, 212))==0 && euclidean_distance(white_pixel, FloatPixel(255, 255, 255))==0){
        return true;
    }

    // No other instances found, did not detect
    return false;
}



HomeListViewWatcher::~HomeListViewWatcher() = default;

HomeListViewWatcher::HomeListViewWatcher(Color color)
    : VisualInferenceCallback("HomeApplicationWatcher")
    , m_detector(color)
{}

void HomeListViewWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);
}

bool HomeListViewWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){

    return m_detector.detect(screen);
}


HomeSummaryViewDetector::~HomeSummaryViewDetector() = default;

HomeSummaryViewDetector::HomeSummaryViewDetector(Color color)
    : m_color(color)
{}

void HomeSummaryViewDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool HomeSummaryViewDetector::detect(const ImageViewRGB32& screen){

    char chars[] = "\n\r—";

    // In most main menus, where the minus button for help shows
    ImageFloatBox minus_help_corner(0.03, 0.965, 0.06, 0.03);
    ImageFloatBox menu_read_corner(0.06, 0.021, 0.25, 0.05);
    std::string minus_button = OCR::ocr_read(Language::English, extract_box_reference(screen, minus_help_corner));
    std::string menu_text = OCR::ocr_read(Language::English, extract_box_reference(screen, menu_read_corner));
    for(auto a:chars){minus_button.erase(std::remove(minus_button.begin(),minus_button.end(), a),minus_button.end());}
    for(auto a:chars){menu_text.erase(std::remove(menu_text.begin(),menu_text.end(), a),menu_text.end());}
    ImageFloatBox top_green(0.36, 0.01, 0.001, 0.001);
    ImageFloatBox top_white(0.36, 0.076, 0.001, 0.001);
    FloatPixel green_pixel = image_stats(extract_box_reference(screen, top_green)).average;
    FloatPixel white_pixel = image_stats(extract_box_reference(screen, top_white)).average;
    if(menu_text == "CHECK SUMMARY" && minus_button == "Help" && euclidean_distance(green_pixel, FloatPixel(149, 248, 212))==0 && euclidean_distance(white_pixel, FloatPixel(255, 255, 255))==0){
        return true;
    }

    // No other instances found, did not detect
    return false;
}



HomeSummaryViewWatcher::~HomeSummaryViewWatcher() = default;

HomeSummaryViewWatcher::HomeSummaryViewWatcher(Color color)
    : VisualInferenceCallback("HomeApplicationWatcher")
    , m_detector(color)
{}

void HomeSummaryViewWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);
}

bool HomeSummaryViewWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){

    return m_detector.detect(screen);
}


HomeMarkingsViewDetector::~HomeMarkingsViewDetector() = default;

HomeMarkingsViewDetector::HomeMarkingsViewDetector(Color color)
    : m_color(color)
{}

void HomeMarkingsViewDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool HomeMarkingsViewDetector::detect(const ImageViewRGB32& screen){

    char chars[] = "\n\r—";

    // In most main menus, where the minus button for help shows
    ImageFloatBox minus_help_corner(0.03, 0.965, 0.06, 0.03);
    ImageFloatBox menu_read_corner(0.06, 0.021, 0.25, 0.05);
    ImageFloatBox markings_box(0.76, 0.325, 0.1, 0.05);
    std::string minus_button = OCR::ocr_read(Language::English, extract_box_reference(screen, minus_help_corner));
    std::string menu_text = OCR::ocr_read(Language::English, extract_box_reference(screen, menu_read_corner));
    std::string markings_text = OCR::ocr_read(Language::English, extract_box_reference(screen, markings_box));
    for(auto a:chars){minus_button.erase(std::remove(minus_button.begin(),minus_button.end(), a),minus_button.end());}
    for(auto a:chars){menu_text.erase(std::remove(menu_text.begin(),menu_text.end(), a),menu_text.end());}
    for(auto a:chars){markings_text.erase(std::remove(markings_text.begin(),markings_text.end(), a),markings_text.end());}
    ImageFloatBox top_green(0.36, 0.01, 0.001, 0.001);
    ImageFloatBox top_white(0.36, 0.076, 0.001, 0.001);
    FloatPixel green_pixel = image_stats(extract_box_reference(screen, top_green)).average;
    FloatPixel white_pixel = image_stats(extract_box_reference(screen, top_white)).average;
    if(markings_text == "Markings" && menu_text == "POKEMON" && minus_button == "Help" && euclidean_distance(green_pixel, FloatPixel(101, 179, 150))==0 && euclidean_distance(white_pixel, FloatPixel(207, 206, 206))==0){
        return true;
    }

    // No other instances found, did not detect
    return false;
}



HomeMarkingsViewWatcher::~HomeMarkingsViewWatcher() = default;

HomeMarkingsViewWatcher::HomeMarkingsViewWatcher(Color color)
    : VisualInferenceCallback("HomeApplicationWatcher")
    , m_detector(color)
{}

void HomeMarkingsViewWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);
}

bool HomeMarkingsViewWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){

    return m_detector.detect(screen);
}


HomeBoxViewDetector::~HomeBoxViewDetector() = default;

HomeBoxViewDetector::HomeBoxViewDetector(Color color)
    : m_color(color)
{}

void HomeBoxViewDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool HomeBoxViewDetector::detect(const ImageViewRGB32& screen){

    char chars[] = "\n\r—";

    // In most main menus, where the minus button for help shows
    ImageFloatBox minus_help_corner(0.03, 0.965, 0.06, 0.03);
    ImageFloatBox menu_read_corner(0.06, 0.021, 0.25, 0.05);
    std::string minus_button = OCR::ocr_read(Language::English, extract_box_reference(screen, minus_help_corner));
    std::string menu_text = OCR::ocr_read(Language::English, extract_box_reference(screen, menu_read_corner));
    for(auto a:chars){minus_button.erase(std::remove(minus_button.begin(),minus_button.end(), a),minus_button.end());}
    for(auto a:chars){menu_text.erase(std::remove(menu_text.begin(),menu_text.end(), a),menu_text.end());}
    ImageFloatBox top_green(0.36, 0.01, 0.001, 0.001);
    ImageFloatBox top_white(0.36, 0.076, 0.001, 0.001);
    FloatPixel green_pixel = image_stats(extract_box_reference(screen, top_green)).average;
    FloatPixel white_pixel = image_stats(extract_box_reference(screen, top_white)).average;
    if(menu_text == "POKEMON" && minus_button == "Help" && euclidean_distance(green_pixel, FloatPixel(149, 248, 212))==0 && euclidean_distance(white_pixel, FloatPixel(255, 255, 255))==0){
        return true;
    }

    // No other instances found, did not detect
    return false;
}



HomeBoxViewWatcher::~HomeBoxViewWatcher() = default;

HomeBoxViewWatcher::HomeBoxViewWatcher(Color color)
    : VisualInferenceCallback("HomeApplicationWatcher")
    , m_detector(color)
{}

void HomeBoxViewWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);
}

bool HomeBoxViewWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){

    return m_detector.detect(screen);
}



HomeLoginDialogueDetector::~HomeLoginDialogueDetector() = default;

HomeLoginDialogueDetector::HomeLoginDialogueDetector(Color color)
    : m_color(color)
{}

void HomeLoginDialogueDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool HomeLoginDialogueDetector::detect(const ImageViewRGB32& screen){

    char chars[] = "\n\r—";

    // Search for the dialogue text to say, "Logging into Pokémon HOME..."
    ImageFloatBox login_dialogue(0.155, 0.82, 0.37, 0.06);
    std::string login_text = OCR::ocr_read(Language::English, extract_box_reference(screen, login_dialogue));
    for(auto a:chars){login_text.erase(std::remove(login_text.begin(),login_text.end(), a),login_text.end());}
    if(login_text == "Logging into Pokémon HOME..."){
        return true;
    }

    // No other instances found, did not detect
    return false;
}



HomeLoginDialogueWatcher::~HomeLoginDialogueWatcher() = default;

HomeLoginDialogueWatcher::HomeLoginDialogueWatcher(Color color)
    : VisualInferenceCallback("HomeApplicationWatcher")
    , m_detector(color)
{}

void HomeLoginDialogueWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);
}

bool HomeLoginDialogueWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){

    return m_detector.detect(screen);
}


HomeLogoutDialogueDetector::~HomeLogoutDialogueDetector() = default;

HomeLogoutDialogueDetector::HomeLogoutDialogueDetector(Color color)
    : m_color(color)
{}

void HomeLogoutDialogueDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool HomeLogoutDialogueDetector::detect(const ImageViewRGB32& screen){

    char chars[] = "\n\r—";

    // Search for the dialogue text to say, "Your Boxes have been saved!"
    ImageFloatBox login_dialogue(0.155, 0.82, 0.37, 0.06);
    std::string login_text = OCR::ocr_read(Language::English, extract_box_reference(screen, login_dialogue));
    for(auto a:chars){login_text.erase(std::remove(login_text.begin(),login_text.end(), a),login_text.end());}
    if(login_text == "Your Boxes have been saved!"){
        return true;
    }

    // No other instances found, did not detect
    return false;
}



HomeLogoutDialogueWatcher::~HomeLogoutDialogueWatcher() = default;

HomeLogoutDialogueWatcher::HomeLogoutDialogueWatcher(Color color)
    : VisualInferenceCallback("HomeApplicationWatcher")
    , m_detector(color)
{}

void HomeLogoutDialogueWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);
}

bool HomeLogoutDialogueWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){

    return m_detector.detect(screen);
}


HomePageRightMoveDetector::~HomePageRightMoveDetector() = default;

HomePageRightMoveDetector::HomePageRightMoveDetector(Color color)
    : m_color(color)
{}

void HomePageRightMoveDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool HomePageRightMoveDetector::detect(const ImageViewRGB32& screen){

    ImageFloatBox page_r_button(0.41, 0.13, 0.001, 0.001);
    FloatPixel pixel = image_stats(extract_box_reference(screen, page_r_button)).average;

    if (pixel.r >= 50) {
        return true;
    }

    return false;
}



HomePageRightMoveWatcher::~HomePageRightMoveWatcher() = default;

HomePageRightMoveWatcher::HomePageRightMoveWatcher(Color color)
    : VisualInferenceCallback("HomeApplicationWatcher")
    , m_detector(color)
{
    m_prev_detected = false;
}

void HomePageRightMoveWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);
}

bool HomePageRightMoveWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){

    if(!m_prev_detected){
        m_prev_detected = m_detector.detect(screen);
        return false;
    }else{
        return !m_detector.detect(screen);
    }}


HomePageLeftMoveDetector::~HomePageLeftMoveDetector() = default;

HomePageLeftMoveDetector::HomePageLeftMoveDetector(Color color)
    : m_color(color)
{}

void HomePageLeftMoveDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool HomePageLeftMoveDetector::detect(const ImageViewRGB32& screen){

    ImageFloatBox page_l_button(0.1, 0.13, 0.001, 0.001);
    FloatPixel pixel = image_stats(extract_box_reference(screen, page_l_button)).average;

    if (pixel.r >= 50) {
        return true;
    }

    return false;
}


HomePageLeftMoveWatcher::~HomePageLeftMoveWatcher() = default;

HomePageLeftMoveWatcher::HomePageLeftMoveWatcher(Color color)
    : VisualInferenceCallback("HomeApplicationWatcher")
    , m_detector(color)
{
    m_prev_detected = false;
}

void HomePageLeftMoveWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);
}

bool HomePageLeftMoveWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){

    if(!m_prev_detected){
        m_prev_detected = m_detector.detect(screen);
        return false;
    }else{
        return !m_detector.detect(screen);
    }

}


HomeSummarySettledDetector::~HomeSummarySettledDetector() = default;

HomeSummarySettledDetector::HomeSummarySettledDetector(Color color)
    : m_color(color)
{
    m_prev_color = FloatPixel(-1,-1,-1);
}

void HomeSummarySettledDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool HomeSummarySettledDetector::detect(const ImageViewRGB32& screen){


    ImageFloatBox pokemon_box_small(0.76, 0.295, 0.14, 0.23);
    FloatPixel m_color = image_stats(extract_box_reference(screen, pokemon_box_small)).average;

    std::cout << std::to_string(m_color.r) << std::to_string(m_color.g) << std::to_string(m_color.b) << std::endl;
    std::cout << std::to_string(m_prev_color.r) << std::to_string(m_prev_color.g) << std::to_string(m_prev_color.b) << std::endl;
    std::cout<< std::to_string(euclidean_distance(m_color,m_prev_color)) << std::endl;


    if(euclidean_distance(m_prev_color,FloatPixel(-1,-1,-1))==0){
        std::cout << "went to force false" << std::endl;
        m_prev_color = m_color;
        return false;
    }

    if (euclidean_distance(m_color,m_prev_color)>0) {
        std::cout << "actual comparison" << std::endl;
        m_prev_color = m_color;
        return true;
    }

    // No other instances found, did not detect
    return false;
}



HomeSummarySettledWatcher::~HomeSummarySettledWatcher() = default;

HomeSummarySettledWatcher::HomeSummarySettledWatcher(Color color)
    : VisualInferenceCallback("HomeApplicationWatcher")
    , m_detector(color)
    , m_still_time(WallClock::min())
{
    m_prev_detected = false;
}

void HomeSummarySettledWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);
}

bool HomeSummarySettledWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){

    if(!m_prev_detected){
        m_prev_detected = m_detector.detect(screen);
        return false;
    }else{
        if(m_detector.detect(screen)){
            return false;
        }else{
            if(m_still_time == WallClock::min()){
                m_still_time = timestamp;
                return false;
            }else{
                return timestamp - m_still_time >= std::chrono::milliseconds(20);
            }
        }
    }
}

class HomeGrabbingCursorMatcher : public ImageMatch::WaterfillTemplateMatcher {
public:
    HomeGrabbingCursorMatcher() : WaterfillTemplateMatcher(
              "PokemonHome/HomeGrabbingHand-Template.png",    // Use the same or replace with correct path
              Color(250,250,250), Color(255, 255, 255), 1300
              ){
        m_aspect_ratio_lower = 0.95;
        m_aspect_ratio_upper = 1.05;
        m_area_ratio_lower = 0.95;
        m_area_ratio_upper = 1.05;
    }

    static const HomeGrabbingCursorMatcher& instance(){
        static HomeGrabbingCursorMatcher matcher;
        return matcher;
    }
};

class HomeRedCursorMatcher : public ImageMatch::WaterfillTemplateMatcher {
public:
    HomeRedCursorMatcher() : WaterfillTemplateMatcher(
              "PokemonHome/HomeRedHand-Template.png",    // Use the same or replace with correct path
              Color(250, 85, 0), Color(255, 93, 5), 50
              ){
        m_aspect_ratio_lower = 0.8;
        m_aspect_ratio_upper = 2;
        m_area_ratio_lower = 0.8;
        m_area_ratio_upper = 1.2;
    }

    static const HomeRedCursorMatcher& instance(){
        static HomeRedCursorMatcher matcher;
        return matcher;
    }
};

HomeCursorLocator::HomeCursorLocator(HomeCursorType cursor_type, const ImageFloatBox& box, Color color)
    : m_type(cursor_type), m_box(box), m_color(color)
{}

void HomeCursorLocator::make_overlays(VideoOverlaySet& items) const{
    // items.add(m_color, m_box);
}

std::pair<int, int> HomeCursorLocator::detect(const ImageViewRGB32& frame) const{
    switch(m_type){
        case HomeCursorType::RED:
            return locate_red_hand(frame, m_box);
            break;
        case HomeCursorType::GREEN:
        case HomeCursorType::BLUE:
        case HomeCursorType::GRABBING:
            return locate_grabbing_hand(frame, m_box);
            break;
        default:
            return {-1, -1};
    }
}

std::pair<int, int> HomeCursorLocator::locate_grabbing_hand(const ImageViewRGB32& frame, const ImageFloatBox& region) const{
    const std::vector<std::pair<uint32_t, uint32_t>> filters = {
        {combine_rgb(250,250,250), combine_rgb(255, 255, 255)}
    };

    const double screen_rel_size = (frame.height() / 1080.0);
    const size_t min_size = static_cast<size_t>(screen_rel_size * screen_rel_size * 1300.0);

    std::pair<double, double> hand_loc(-1, -1);

    ImagePixelBox pixel_box = floatbox_to_pixelbox(frame.width(), frame.height(), region);
    match_template_by_waterfill(
        frame.size(),
        extract_box_reference(frame, region),
        HomeGrabbingCursorMatcher::instance(),
        filters,
        {min_size, SIZE_MAX},
        80,
        [&](Kernels::Waterfill::WaterfillObject& object) -> bool {
            hand_loc = std::make_pair(
                (object.center_of_gravity_x() + pixel_box.min_x) / static_cast<double>(frame.width()),
                (object.center_of_gravity_y() + pixel_box.min_y) / static_cast<double>(frame.height())
                );
            return true;  // stop at first match
        }
        );

    return {
        (int)((hand_loc.first  - 0.07) / 0.0718714),
        (int)((hand_loc.second - 0.15) / 0.10555925)
    };


}

std::pair<int, int> HomeCursorLocator::locate_red_hand(const ImageViewRGB32& frame, const ImageFloatBox& region) const{
    const std::vector<std::pair<uint32_t, uint32_t>> filters = {
        {combine_rgb(250, 85, 0), combine_rgb(255, 93, 5)}
    };

    const double screen_rel_size = (frame.height() / 1080.0);
    const size_t min_size = static_cast<size_t>(screen_rel_size * screen_rel_size * 50);

    std::pair<double, double> hand_loc(-1, -1);

    ImagePixelBox pixel_box = floatbox_to_pixelbox(frame.width(), frame.height(), region);
    match_template_by_waterfill(
        frame.size(),
        extract_box_reference(frame, region),
        HomeRedCursorMatcher::instance(),
        filters,
        {min_size, SIZE_MAX},
        40,
        [&](Kernels::Waterfill::WaterfillObject& object) -> bool {
            hand_loc = std::make_pair(
                (object.center_of_gravity_x() + pixel_box.min_x) / static_cast<double>(frame.width()),
                (object.center_of_gravity_y() + pixel_box.min_y) / static_cast<double>(frame.height())
                );
            return true;  // stop at first match
        }
        );

    return {
        (int)((hand_loc.first  - 0.07) / 0.0718714),
        (int)((hand_loc.second - 0.15) / 0.10555925)
    };


}



HomeCursorWatcher::HomeCursorWatcher(const HomeCursorType cursor_type, const ImageFloatBox& box, Color color)
    : VisualInferenceCallback(cursor_type==HomeCursorType::GRABBING?"HomeGrabbingHandWatcher":"HomeRedHandWatcher")
    , m_locator(cursor_type, box, color)
    , m_location(-1, -1)
{}

void HomeCursorWatcher::make_overlays(VideoOverlaySet& items) const{
    m_locator.make_overlays(items);
}

bool HomeCursorWatcher::process_frame(const ImageViewRGB32& frame, WallClock timestamp){
    m_location = m_locator.detect(frame);
    return m_location.first >= 0.0;
}


NameBoxDetector::~NameBoxDetector() = default;

NameBoxDetector::NameBoxDetector(Color color)
    : m_color(color)
{
}

void NameBoxDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool NameBoxDetector::detect(const ImageViewRGB32& screen){

    char chars[] = "\n\r—";

    ImageFloatBox boxName(0.102, 0.075, 0.408, 0.045);
    std::string box_name_text = OCR::ocr_read(Language::English, extract_box_reference(screen, boxName));
    for(auto a:chars){box_name_text.erase(std::remove(box_name_text.begin(),box_name_text.end(), a),box_name_text.end());}
    if(box_name_text == "What would you like to name this Box?"|| box_name_text == "VWhat would you like to name this Box?"){
        return true;
    }

    return false;

}



NameBoxWatcher::~NameBoxWatcher() = default;

NameBoxWatcher::NameBoxWatcher(Color color)
    : VisualInferenceCallback("HomeApplicationWatcher")
    , m_detector(color)
    , m_still_time(WallClock::min())
{
    m_prev_detected = false;
}

void NameBoxWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);
}

bool NameBoxWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){

    return m_detector.detect(screen);

}


class FilterCursorMatcher : public ImageMatch::WaterfillTemplateMatcher {
public:
    FilterCursorMatcher() : WaterfillTemplateMatcher(
              "PokemonHome/HomeFilterIcon-Template.png",    // Use the same or replace with correct path
              Color(75, 205, 205), Color(95, 220, 220), 50
              ){
        m_aspect_ratio_lower = 0.8;
        m_aspect_ratio_upper = 2;
        m_area_ratio_lower = 0.8;
        m_area_ratio_upper = 1.4;
    }

    static const FilterCursorMatcher& instance(){
        static FilterCursorMatcher matcher;
        return matcher;
    }
};


FilterCursorLocator::FilterCursorLocator(const ImageFloatBox& box, Color color)
    : m_box(box), m_color(color)
{}

void FilterCursorLocator::make_overlays(VideoOverlaySet& items) const{
    // items.add(m_color, m_box);
}

std::pair<double, double> FilterCursorLocator::detect(const ImageViewRGB32& frame) const{
    const std::vector<std::pair<uint32_t, uint32_t>> filters = {
        {combine_rgb(75, 205, 205), combine_rgb(95, 220, 220)}
    };

    const double screen_rel_size = (frame.height() / 1080.0);
    const size_t min_size = static_cast<size_t>(screen_rel_size * screen_rel_size * 50);

    std::pair<double, double> hand_loc(-1, -1);

    ImagePixelBox pixel_box = floatbox_to_pixelbox(frame.width(), frame.height(), m_box);
    match_template_by_waterfill(
        frame.size(),
        extract_box_reference(frame, m_box),
        FilterCursorMatcher::instance(),
        filters,
        {min_size, SIZE_MAX},
        40,
        [&](Kernels::Waterfill::WaterfillObject& object) -> bool {
            hand_loc = std::make_pair(
                (object.center_of_gravity_x() + pixel_box.min_x) / static_cast<double>(frame.width()),
                (object.center_of_gravity_y() + pixel_box.min_y) / static_cast<double>(frame.height())
                );
            return true;  // stop at first match
        }
        );

    return hand_loc;

}



FilterCursorWatcher::FilterCursorWatcher(const ImageFloatBox& box, Color color)
    : VisualInferenceCallback("FilterCursorWatcher")
    , m_locator(box, color)
    , m_location(-1, -1)
{}

void FilterCursorWatcher::make_overlays(VideoOverlaySet& items) const{
    m_locator.make_overlays(items);
}

bool FilterCursorWatcher::process_frame(const ImageViewRGB32& frame, WallClock timestamp){
    m_location = m_locator.detect(frame);
    return m_location.first >= 0.0;
}

}
}
}
