#include "PokemonHome_SummaryDetector.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/OCR/OCR_NumberReader.h"
#include "PokemonHome/Inference/PokemonHome_AbilityReader.h"
#include "PokemonHome/Inference/PokemonHome_BoxGenderDetector.h"
#include <unordered_map>

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
    return true;
}

PokemonData SummaryDetector::identify_pokemon(Logger& logger, const ImageViewRGB32& screen, PokedexReader& information){
    ImageFloatBox national_dex_number_box(0.448, 0.245, 0.042, 0.04);
    ImageFloatBox shiny_symbol_box(0.702, 0.09, 0.04, 0.06);
    ImageFloatBox gmax_symbol_box(0.463, 0.09, 0.04, 0.06);
    ImageFloatBox pokemon_box_small(0.76, 0.295, 0.14, 0.23);
    ImageFloatBox origin_symbol_box(0.623, 0.095, 0.033, 0.05);
    ImageFloatBox level_box(0.546, 0.099, 0.044, 0.041);
    ImageFloatBox type_1(0.622, 0.245, 0.029, 0.053);
    ImageFloatBox type_2(0.654, 0.245, 0.029, 0.053);
    ImageFloatBox ot_id_box(0.782, 0.719, 0.193, 0.046);
    ImageFloatBox ability_box(0.17, 0.84, 0.215, 0.044);

    auto closest_type = [&](const FloatPixel& color_box) -> PokemonType{
        static const std::pair<PokemonType, Color> type_color_list[] = {
            {PokemonType::GRASS, Color(62, 180, 86)}, {PokemonType::FIRE, Color(201, 106, 83)}, {PokemonType::WATER, Color(31, 161, 243)},
            {PokemonType::ELECTRIC, Color(202, 207, 66)}, {PokemonType::ROCK, Color(164, 201, 169)}, {PokemonType::GROUND, Color(145, 130, 78)},
            {PokemonType::POISON, Color(142, 125, 234)}, {PokemonType::DARK, Color(86, 118, 113)}, {PokemonType::STEEL, Color(91, 188, 211)},
            {PokemonType::FLYING, Color(112, 206, 242)}, {PokemonType::NORMAL, Color(146, 190, 186)}, {PokemonType::FIGHTING, Color(203, 173, 82)},
            {PokemonType::GHOST, Color(116, 125, 157)}, {PokemonType::DRAGON, Color(80, 145, 241)}, {PokemonType::ICE, Color(44, 224, 243)},
            {PokemonType::FAIRY, Color(202, 166, 242)}, {PokemonType::BUG, Color(140, 188, 87)}, {PokemonType::PSYCHIC, Color(202, 128, 156)},
            {PokemonType::NONE, Color(20, 191, 195)}
        };

        double min_distance = std::numeric_limits<double>::max();
        PokemonType closest_type = PokemonType::NONE;

        for (const auto& [type, color] : type_color_list) {
            double distance = euclidean_distance(color_box, color);
            if (distance < min_distance) {
                min_distance = distance;
                closest_type = type;
            }
        }

        return closest_type;
    };

    AbilityReader ability_reader(Language::English);


    FloatPixel type_1_color = image_stats(extract_box_reference(screen, type_1)).average;
    FloatPixel type_2_color = image_stats(extract_box_reference(screen, type_2)).average;
    PokemonType type1 = closest_type(type_1_color);
    PokemonType type2 = closest_type(type_2_color);
    // FloatPixel pokemon_color = image_stats(extract_box_reference(screen, pokemon_box_small)).average;    // TODO: Use this for multiple matches
    int shiny_stddev_value = static_cast<int>(image_stddev(extract_box_reference(screen, shiny_symbol_box)).sum());
    int gmax_stddev_value = static_cast<int>(image_stddev(extract_box_reference(screen, gmax_symbol_box)).sum());
    int ot_id_value = OCR::read_number_waterfill(logger, extract_box_reference(screen, ot_id_box), 0xff808080, 0xffffffff);
    int national_dex_number = OCR::read_number_waterfill(logger, extract_box_reference(screen, national_dex_number_box), 0xff808080, 0xffffffff);
    StatsHuntGenderFilter gender = BoxGenderDetector::detect(screen);
    int level = OCR::read_number_waterfill(logger, extract_box_reference(screen, level_box), 0xff000000, 0xff7f7f7f);
    std::string ability = ability_reader.read_ability(logger, extract_box_reference(screen, ability_box));

    std::vector<PokemonInformation> matches = PokemonInformation().Id(national_dex_number).Gender(gender).Type1(type1).Type2(type2).Ability({ability}).match(information.get_pokedex());

    if(matches.size()==1){
        return PokemonData(
            national_dex_number,
            matches[0].form_id.value(),
            matches[0].form.value(),
            gender,
            type1,
            type2,
            matches[0].region.value(),
            ot_id_value,
            level,
            shiny_stddev_value > 30,
            gmax_stddev_value > 30,
            ability,
            PokemonType::NONE, // TODO: Add tera typing
            false
            );
    }else{
        FloatPixel pokemon_color = image_stats(extract_box_reference(screen, pokemon_box_small)).average;

        static std::unordered_map<int, std::vector<std::pair<FloatPixel, FloatPixel>>> visual_forms = {
            // {25, {{}}} // Pikachu (Base+Shiny, Cap, Hoenn, Sinnoh, Unova, Kalos, Alola, Partner, World)
            {201, {{FloatPixel(170.934285, 172.982582, 171.41171), /*Need shiny A form Unown*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(171.051985, 172.886542, 171.652027), /*Need shiny B form Unown*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(176.412055, 178.22759, 176.8081), /*Need shiny C form Unown*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(159.567979, 161.690026, 160.454821), FloatPixel(157.188212, 198.651457, 236.569283)},
                   {FloatPixel(160.319388, 162.789918, 161.06639), /*Need shiny E form Unown*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(153.205915, 155.198765, 154.417376), FloatPixel(150.810034, 193.6098, 235.118375)},
                   {FloatPixel(182.024254, 183.959737, 182.321307), FloatPixel(180.19163, 211.312942, 238.684495)},
                   {FloatPixel(162.561158, 164.912954, 163.358196), /*Need shiny H form Unown*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(173.638146, 175.842502, 174.158502), /*Need shiny I form Unown*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(174.78719, 176.783337, 175.370398), FloatPixel(173.128448, 205.976991, 236.628388)},
                   {FloatPixel(155.181497, 157.789063, 156.005996), FloatPixel(152.902236, 195.905834, 234.782663)},
                   {FloatPixel(182.587585, 184.501769, 183.053828), FloatPixel(180.942259, 210.843551, 238.327138)},
                   {FloatPixel(165.695827, 167.787549, 166.355483), /*Need shiny M form Unown*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(156.977935, 159.123696, 157.581949), /*Need shiny N form Unown*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(168.583553, 170.585142, 169.121477), FloatPixel(166.553304, 203.350642, 236.466633)},
                   {FloatPixel(165.417406, 167.340598, 166.266998), /*Need shiny P form Unown*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(157.113683, 158.939366, 157.916402), FloatPixel(154.486959, 193.69863, 232.002593)},
                   {FloatPixel(150.893273, 152.667826, 151.728475), FloatPixel(148.442904, 191.789708, 233.815925)},
                   {FloatPixel(171.734725, 173.602096, 172.299721), FloatPixel(169.83889, 206.225567, 238.884264)},
                   {FloatPixel(183.944942, 185.595455, 184.330615), FloatPixel(182.191525, 213.09559, 240.793006)},
                   {FloatPixel(149.057921, 150.999191, 149.797233), /*Need shiny U form Unown*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(174.859411, 176.782708, 175.724967), FloatPixel(173.018827, 209.631461, 241.697251)},
                   {FloatPixel(174.45659, 176.309105, 174.934824), FloatPixel(172.27166, 207.564951, 239.244499)},
                   {FloatPixel(153.545989, 155.090164, 154.541672), /*Need shiny X form Unown*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(178.924631, 180.80897, 179.51523), /*Need shiny Y form Unown*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(161.410736, 163.732717, 162.055058), FloatPixel(159.08811, 198.56639, 234.025063)},
                   {FloatPixel(166.253478, 167.907453, 166.422323), /*Need shiny ! form Unown*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(162.93385, 164.838815, 163.50664), FloatPixel(160.774164, 205.105798, 242.707804)}}}, // Unown (A-Z?!+Shiny)
            // {351, {{}}} // Castform (Normal+Shiny, Sunny+Shiny, Rainy+Shiny, Snowy+Shiny)
            // {386, {{}}} // Deoxys (Normal+Shiny, Attack+Shiny, Defense+Shiny, Speed+Shiny)
            {412, {{FloatPixel(187.917961, 224.88263, 150.888176), FloatPixel(186.449005, 226.713005, 155.512037)},
                   {FloatPixel(234.216738, 234.237543, 209.198765), FloatPixel(232.057786, 236.224607, 215.107357)},
                   {FloatPixel(238.852021, 218.341228, 220.157693), FloatPixel(236.174496, 219.359965, 224.627593)}}}, // Burmy (Plant+Shiny, Sandy+Shiny, Trash+Shiny)
            {413, {{FloatPixel(167.571861, 212.879677, 158.623366), FloatPixel(167.632945, 215.726481, 164.212975)},
                   {FloatPixel(227.790158, 209.376304, 177.705915), FloatPixel(227.686578, 212.199439, 183.602096)},
                   {/*Need nonshiny Trash Cloak form Wormadam*/ FloatPixel(255, 255, 255), FloatPixel(240.343821, 199.945587, 213.737379)}}}, // Wormadam (Plant+Shiny, Sandy+Shiny, Trash+Shiny)
            {422, {{FloatPixel(123.741276, 214.299886, 216.025723), /*Need shiny East sea form Shellos*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(249.065206, 203.930327, 199.138116), /*Need shiny West sea form Shellos*/ FloatPixel(255, 255, 255)}}}, // Shellos (East Sea+Shiny, West Sea+Shiny)
            {423, {{FloatPixel(115.69842, 205.94094, 167.599727), /*Need shiny East sea form Gastrodon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(223.847374, 184.596384, 159.035166), FloatPixel(221.540532, 199.802884, 136.430507)}}}, // Gastrodon (East Sea+Shiny, West Sea+Shiny)
            {550, {{FloatPixel(154.554668, 184.862948, 128.165682), FloatPixel(176.424991, 220.055567, 139.549646)},
                   {FloatPixel(155.520356, 196.498486, 152.874385), FloatPixel(173.566929, 223.443818, 153.306796)},
                   {FloatPixel(155.77202, 182.861839, 141.182531), FloatPixel(176.417361, 215.793141, 147.944463)}}}, // Basculin (Red Striped+Shiny, Blue Striped+Shiny, White Striped+Shiny)
            {585, {{FloatPixel(248.905624, 231.960547, 202.304008), /*Need shiny Spring Form form Deerling*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(200.765784, 221.649718, 157.010388), /*Need shiny Summer Form form Deerling*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(249.129347, 223.596864, 153.540712), /*Need shiny Autumn Form form Deerling*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny Winter Form form Deerling*/ FloatPixel(255, 255, 255), /*Need shiny Winter Form form Deerling*/ FloatPixel(255, 255, 255)}}}, // Deerling (Spring+Shiny, Summer+Shiny, Autumn+Shiny, Winter+Shiny)
            {586, {{FloatPixel(226.703307, 210.355288, 183.036095), /*Need shiny Spring Form form Sawsbuck*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny Summer Form form Sawsbuck*/ FloatPixel(255, 255, 255), /*Need shiny Summer Form form Sawsbuck*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(225.556512, 191.422413, 157.115032), /*Need shiny Autumn Form form Sawsbuck*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(230.642283, 221.722029, 206.291687), /*Need shiny Winter Form form Sawsbuck*/ FloatPixel(255, 255, 255)}}}, // Sawsbuck (Spring+Shiny, Summer+Shiny, Autumn+Shiny, Winter+Shiny)
            {641, {{FloatPixel(180.638956, 187.422533, 160.762681), /*Need shiny Incarnate form Tornadus*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny Therian form Tornadus*/ FloatPixel(255, 255, 255), /*Need shiny Therian form Tornadus*/ FloatPixel(255, 255, 255)}}}, // Tornadus (Incarnate+Shiny, Therian+Shiny)
            {642, {{FloatPixel(181.380142, 198.698165, 205.539588), /*Need shiny Incarnate form Thundurus*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(190.639990,213.828007, 226.926490), FloatPixel(217.579161, 205.614537, 235.753148)}}}, // Thundurus (Incarnate+Shiny, Therian+Shiny)
            {645, {{FloatPixel(216.922398, 184.366681, 141.085397), /*Need shiny Incarnate form Landorus*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(234.314036,198.174901,152.318818), /*Need shiny Therian form Landorus*/ FloatPixel(255, 255, 255)}}}, // Landorus (Incarnate+Shiny, Therian+Shiny)
            // {647, {{}}} // Keldeo (Normal+Shiny, Resolute+Shiny)
            {666, {{FloatPixel(221.886572, 169.684854, 191.849577), /*Need shiny Meadow form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(231.026757, 231.861194, 230.119484), /*Need shiny Icy Snow form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(95.708298, 134.220905, 214.867970), /*Need shiny Polar form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(185.650887, 224.292676, 234.018437), /*Need shiny Tundra form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(227.867925, 208.871013, 81.038149), /*Need shiny Continental form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(85.099892, 182.975282, 163.189576), /*Need shiny Garden form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(142.155684, 126.404845, 171.699304), /*Need shiny Elegant form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(217.237603, 114.672443, 100.138671), /*Need shiny Modern form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(91.567334, 189.329566, 231.155309), /*Need shiny Marine form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(214.911440, 141.001184, 78.027551), /*Need shiny Archipelago form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(217.478939, 180.618524, 102.546124), /*Need shiny High Plains form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(222.375989, 201.872392, 154.396795), /*Need shiny Sandstorm form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(190.078921, 178.329581, 131.191555), /*Need shiny River form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(167.391114, 182.842832, 186.364297), /*Need shiny Monsoon form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(151.286890, 206.048972, 170.821307), /*Need shiny Savanna form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(233.181227, 176.330345, 98.137022), /*Need shiny Sun form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(208.253942, 199.788344, 134.279350), /*Need shiny Ocean form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(132.258709, 164.699424, 148.602365), /*Need shiny Jungle form Vivillon*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(210.645851, 216.916552, 186.039003), /*Need shiny Fancy form Vivillon*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny Pokeball form Vivillon*/ FloatPixel(255, 255, 255), /*Need shiny Pokeball form Vivillon*/ FloatPixel(255, 255, 255)}}}, // Vivillon
            {669, {{/*Need nonshiny Red form Flabebe*/ FloatPixel(255, 255, 255), /*Need shiny Red form Flabebe*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny Blue form Flabebe*/ FloatPixel(255, 255, 255), /*Need shiny Blue form Flabebe*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny Orange form Flabebe*/ FloatPixel(255, 255, 255), /*Need shiny Orange form Flabebe*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny White form Flabebe*/ FloatPixel(255, 255, 255), /*Need shiny White form Flabebe*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(240.832009, 240.773714, 154.978714), /*Need shiny Yellow form Flabebe*/ FloatPixel(255, 255, 255)}}}, // Flabebe (Red+Shiny, Yellow+Shiny, Orange+Shiny, Blue+Shiny, White+Shiny)
            {670, {{FloatPixel(229.313722, 220.905609, 194.702512), /*Need shiny Red form Floette*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny Blue form Floette*/ FloatPixel(255, 255, 255), /*Need shiny Blue form Floette*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(229.489852, 230.593327, 186.307906), /*Need shiny Orange form Floette*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(228.545479, 243.563662, 213.230393), /*Need shiny White form Floette*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(229.107357, 240.968701, 195.879662), /*Need shiny Yellow form Floette*/ FloatPixel(255, 255, 255)}}}, // Floette (Red+Shiny, Yellow+Shiny, Orange+Shiny, Blue+Shiny, White+Shiny)
            {671, {{FloatPixel(183.045869, 202.187822, 195.971864), /*Need shiny Red form Florges*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny Blue form Florges*/ FloatPixel(255, 255, 255), /*Need shiny Blue form Florges*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny Orange form Florges*/ FloatPixel(255, 255, 255), /*Need shiny Orange form Florges*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(181.038704, 230.859156, 221.319118), /*Need shiny White form Florges*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny Yellow form Florges*/ FloatPixel(255, 255, 255), /*Need shiny Yellow form Florges*/ FloatPixel(255, 255, 255)}}}, // Florges (Red+Shiny, Yellow+Shiny, Orange+Shiny, Blue+Shiny, White+Shiny)
            {676, {{FloatPixel(240.378433, 239.426400, 229.896450), /*Need shiny Natural form Furfrou*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(218.810469, 206.982612, 200.879512), /*Need shiny Heart form Furfrou*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(199.981742, 222.200114, 216.318773), /*Need shiny Star form Furfrou*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(226.835382, 214.561173, 180.754962), /*Need shiny Diamond form Furfrou*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny Debutante form Furfrou*/ FloatPixel(255, 255, 255), /*Need shiny Debutante form Furfrou*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(229.316555, 213.887711, 211.874595), /*Need shiny Matron form Furfrou*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(210.150992, 222.111044, 182.765754), /*Need shiny Dandy form Furfrou*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny La Reine form Furfrou*/ FloatPixel(255, 255, 255), /*Need shiny La Reine form Furfrou*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(228.011032, 185.871507, 162.710187), /*Need shiny Kabuki form Furfrou*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny Pharoah form Furfrou*/ FloatPixel(255, 255, 255), /*Need shiny Pharoah form Furfrou*/ FloatPixel(255, 255, 255)}}}, // Furfrou (Normal+Shiny, Heart+Shiny, Star+Shiny, Diamond+Shiny, Debutante+Shiny, Matron+Shiny, Dandy+Shiny, La Reine+Shiny, Kabuki+Shiny, Pharoah+Shiny)
            // {710, {{}}} // Pumpkaboo (Average+Shiny, Small+Shiny, Large+Shiny, Super+Shiny)
            // {711, {{}}} // Gourgeist (Average+Shiny, Small+Shiny, Large+Shiny, Super+Shiny)
            // {720, {{}}} // Hoopa (Confined+Shiny, Unbound+Shiny)
            // {745, {{}}} // Lycanroc (Midday+Shiny, Midnight+Shiny, Dusk+Shiny)
            // {774, {{}}} // Minior
            // {801, {{}}} // Magearna
            {849, {{FloatPixel(214.090913, 198.908907, 201.775168), /*Need shiny Amped Form form Toxtricity*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(208.276217, 213.869694, 236.102680), /*Need shiny Low Key Form form Toxtricity*/ FloatPixel(255, 255, 255)}}}, // Toxtricity
            {869, {{{/*Need nonshiny vanilla-cream-strawberry-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny vanilla-cream-strawberry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(253.648924, 227.252368, 235.011827), /*Need shiny ruby-cream-strawberry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny matcha-cream-strawberry-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny matcha-cream-strawberry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(228.571336, 235.559315, 236.906314), /*Need shiny mint-cream-strawberry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny lemon-cream-strawberry-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny lemon-cream-strawberry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny salted-cream-strawberry-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny salted-cream-strawberry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny ruby-swirl-strawberry-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny ruby-swirl-strawberry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny caramel-swirl-strawberry-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny caramel-swirl-strawberry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny rainbow-swirl-strawberry-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny rainbow-swirl-strawberry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny vanilla-cream-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny vanilla-cream-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(233.593342, 228.645326, 244.782588), /*Need shiny ruby-cream-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny matcha-cream-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny matcha-cream-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny mint-cream-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny mint-cream-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny lemon-cream-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny lemon-cream-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny salted-cream-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny salted-cream-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny ruby-swirl-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny ruby-swirl-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny caramel-swirl-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny caramel-swirl-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny rainbow-swirl-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny rainbow-swirl-berry-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(228.011032, 185.871507, 162.710187), /*Need shiny vanilla-cream-love-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny ruby-cream-love-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny ruby-cream-love-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny matcha-cream-love-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny matcha-cream-love-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny mint-cream-love-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny mint-cream-love-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny lemon-cream-love-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny lemon-cream-love-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny salted-cream-love-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny salted-cream-love-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny ruby-swirl-love-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny ruby-swirl-love-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny caramel-swirl-love-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny caramel-swirl-love-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny rainbow-swirl-love-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny rainbow-swirl-love-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny vanilla-cream-star-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny vanilla-cream-star-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny ruby-cream-star-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny ruby-cream-star-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny matcha-cream-star-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny matcha-cream-star-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny mint-cream-star-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny mint-cream-star-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny lemon-cream-star-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny lemon-cream-star-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny salted-cream-star-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny salted-cream-star-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny ruby-swirl-star-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny ruby-swirl-star-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(253.548267, 243.189171, 178.852605), /*Need shiny caramel-swirl-star-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny rainbow-swirl-star-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny rainbow-swirl-star-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny vanilla-cream-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny vanilla-cream-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny ruby-cream-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny ruby-cream-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny matcha-cream-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny matcha-cream-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny mint-cream-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny mint-cream-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny lemon-cream-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny lemon-cream-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny salted-cream-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny salted-cream-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny ruby-swirl-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny ruby-swirl-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny caramel-swirl-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny caramel-swirl-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny rainbow-swirl-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny rainbow-swirl-clover-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny vanilla-cream-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny vanilla-cream-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny ruby-cream-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny ruby-cream-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny matcha-cream-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny matcha-cream-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny mint-cream-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny mint-cream-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny lemon-cream-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny lemon-cream-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny salted-cream-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny salted-cream-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny ruby-swirl-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny ruby-swirl-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny caramel-swirl-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny caramel-swirl-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny rainbow-swirl-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny rainbow-swirl-flower-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny vanilla-cream-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny vanilla-cream-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny ruby-cream-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny ruby-cream-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny matcha-cream-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny matcha-cream-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny mint-cream-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny mint-cream-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny lemon-cream-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny lemon-cream-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny salted-cream-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny salted-cream-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny ruby-swirl-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny ruby-swirl-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny caramel-swirl-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny caramel-swirl-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255)},
                   {/*Need nonshiny rainbow-swirl-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255), /*Need shiny rainbow-swirl-ribbon-sweet form Alcremie*/ FloatPixel(255, 255, 255)}}}},
            // {893, {{}}} // Zarude
            // {901, {{}}} // Ursaluna
            // {905, {{}}} // Enamorus (Incarnate+Shiny, Therian+Shiny)
            // {925, {{}}} // Maushold
            // {931, {{}}} // Squawkabilly
            // {978, {{}}} // Tatsugiri
            // {982, {{}}} // Dudunsparce
            {999, {{FloatPixel(222.680417, 208.518917, 169.581170), /*Need shiny Chest form Gimmighoul*/ FloatPixel(255, 255, 255)},
                   {FloatPixel(227.996058, 232.016789, 224.896271), /*Need shiny Roaming form Gimmighoul*/ FloatPixel(255, 255, 255)}}} // Gimmighoul
        };

        std::unordered_map<int, std::pair<bool,bool>> issue_list = {
            {854,{true,true}},
            {855,{true,true}},
            {1012,{true,true}},
            {1013,{true,true}}
        };


        // The only forms left are visual forms, although shiny alcremie throws a huge wrench in things. Not sure how we'll check that.
        // I'm thinking set the form to -1, we can use a system that encodes the last 4 symbols (square, heart, star, diamond) to store the form
        // This will only be allowed for a subset of pokemon, which we can store in a player-specific txt file. if the ot and date met match, we can allow it as marked, otherwise, nothing should be marked.
        int form_id = -2;

        if((shiny_stddev_value > 30)?issue_list[national_dex_number].second:issue_list[national_dex_number].first){ // Shiny alcremie, all sinistea, etc.
            return PokemonData(
                national_dex_number,
                -1,
                matches[0].form.value(),
                gender,
                type1,
                type2,
                matches[0].region.value(),
                ot_id_value,
                level,
                shiny_stddev_value > 30,
                gmax_stddev_value > 30,
                ability,
                PokemonType::NONE, // TODO: Add tera typing
                false
                );
            // throw std::logic_error("Unidentifiable form encountered. Unknown solution.");
        }else{
            auto& form_candidates = visual_forms[national_dex_number];

            int i = 0;
            for(auto form_guess: form_candidates){
                if(euclidean_distance((shiny_stddev_value > 30)? form_guess.second:form_guess.first, pokemon_color)<10){
                    form_id = i;
                }
                i++;
            }

            if(form_id == -2){
                logger.log(std::to_string(pokemon_color.r)+", "+std::to_string(pokemon_color.g)+", "+std::to_string(pokemon_color.b));
                throw std::logic_error("Could not identify visual form. RGB Values for extracted box are: ("+std::to_string(pokemon_color.r)+","+std::to_string(pokemon_color.g)+","+std::to_string(pokemon_color.b)+")");
            }


            return PokemonData(
                national_dex_number,
                form_id,
                matches[form_id].form.value(),
                gender,
                type1,
                type2,
                matches[form_id].region.value(),
                ot_id_value,
                level,
                shiny_stddev_value > 30,
                gmax_stddev_value > 30,
                ability,
                PokemonType::NONE, // TODO: Add tera typing
                false
                );

        }



    }

}



SummaryWatcher::~SummaryWatcher() = default;

SummaryWatcher::SummaryWatcher(Color color)
    : VisualInferenceCallback("SummaryWatcher")
    , m_detector(color)
    , m_frozen_screen(
          COLOR_CYAN,                      // overlay color for frozen detection
          ImageFloatBox(0.0, 0.0, 1.0, 1.0), // monitor full screen; adjust if needed
          std::chrono::milliseconds(100),         // wait 0.1 seconds of inactivity
          5.0                              // RMSD threshold
          )
{}

// Draw both overlays for debugging
void SummaryWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);      // summary detection boxes
    m_frozen_screen.make_overlays(items); // frozen screen box
}

// Only return true when the screen is frozen AND the summary is detected
bool SummaryWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){
    // Step 1: Check if the screen is frozen
    bool frozen = m_frozen_screen.process_frame(screen, timestamp);

    // Step 2: Only check for summary if screen is frozen
    if (!frozen){
        return false;
    }

    // Step 3: Check if the summary screen is actually present
    return m_detector.detect(screen);
}

// Get the Pokémon info using the SummaryDetector
PokemonData SummaryWatcher::get_pokemon(Logger& logger, const ImageViewRGB32& screen, PokedexReader& information){
    return m_detector.identify_pokemon(logger, screen, information);
}


}
}
}
