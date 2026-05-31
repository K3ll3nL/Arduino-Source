"""
Given a file saved as:

National,Slug,Name,ja-Hrkt,roomaji,ko,zh-Hant,fr,de,es,it,en,ja,zh-Hans,rus,tha
1007,koraidon,Koraidon,コライドン,,코라이돈,故勒頓,Koraidon,Koraidon,Koraidon,Koraidon,Koraidon,コライドン,故勒顿,Корайдон,โคไรดอน
1008,miraidon,Miraidon,ミライドン,,미라이돈,密勒頓,Miraidon,Miraidon,Miraidon,Miraidon,Miraidon,ミライドン,密勒顿,Мирайдон,มิไรดอน
...

collected by Denv, run this script to convert the above content into JSON format used in
Resources/Pokemon/PokemonNameDisplay.json
{
    ...
    "pokemon-slug": {
        "eng": "Pokemon Name",
        "jpn": ...
        ...
    },
    ...
}
"""

from typing import List, Dict
import json

with open(f"pokemon_displays.txt", "r", encoding='utf-8') as f:
    lines: List[str] = f.readlines()

language_line: str = lines[0]
input_lan_names: List[str] = language_line.strip().split(',')

output_lan_names = ['eng', 'jpn', 'spa', 'fra', 'deu', 'ita', 'kor', 'chi_sim', 'chi_tra']

# output lan name -> input lan name
lan_name_map: Dict[str, str] = {
    'eng': 'en',
    'jpn': 'ja',
    'spa': 'es',
    'fra': 'fr',
    'deu': 'de',
    'ita': 'it',
    'kor': 'ko',
    'chi_sim': 'zh-Hans',
    'chi_tra': 'zh-Hant',
}
# output lan name -> the language's index in `input_lan_names`
lan_indices: Dict[str, int] = {output_lan: input_lan_names.index(input_lan) for output_lan, input_lan in lan_name_map.items()}

output_json = {}
for line in lines[1:]:
    words = line.strip().split(',')
    names = {
        l: words[lan_indices[l]] for l in lan_name_map
    }
    # print(names)
    slug = names['eng'].lower().replace(' ', '-')
    output_json[slug] = names

print(output_json)
with open(f'pokemon_displays.json', 'w', encoding='utf-8') as f:
    json.dump(output_json, f, ensure_ascii=False, indent=4)