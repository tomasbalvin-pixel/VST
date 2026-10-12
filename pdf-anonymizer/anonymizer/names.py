"""Word lists used by the personal-name heuristics.

All entries are compared after ``fold()`` (lower case, diacritics removed).
Czech and Slovak first names are expanded with their declined forms
(Tomáš -> Tomáše, Tomášovi, Tomášem ...), because names in Czech text rarely
appear in the nominative only.
"""

from .textutil import fold

_CZECH_SK_MALE = """
Jan Petr Josef Pavel Martin Jaroslav Tomáš Miroslav Jiří Zdeněk Michal František
Václav Karel Milan Vladimír Lukáš Jakub David Ondřej Ladislav Roman Stanislav Marek
Radek Daniel Antonín Vojtěch Filip Adam Matěj Dominik Patrik Aleš Libor Vít Rudolf
Jindřich Miloslav Oldřich Luboš Jaromír Vlastimil Bohumil Ivan Štěpán Robert Richard
Igor Viktor Kamil Šimon Denis Matyáš Ondra Honza Pepa Mirek Jirka Zdenda Otakar
Bohuslav Bedřich Emil Eduard Arnošt Leoš Dušan Rostislav Radim Radovan Ivo Svatopluk
Přemysl Kryštof Tadeáš Sebastián Vojta Mikuláš Hynek Erik Alois Ludvík Lubomír
Břetislav Bořivoj Čestmír Dalibor Drahomír Ferdinand Hubert Ignác Jáchym Jonáš Kevin
Lumír Marian Matouš Metoděj Norbert Otto Prokop Radomír Samuel Teodor Tobiáš Valentýn
Viliam Vratislav Zbyněk Zbyšek Zikmund Juraj Peter Ján Jozef Ľubomír Matúš Marián
Štefan Ondrej Branislav Tibor Rastislav Ľudovít Miloš Vladislav Jaroslaw Bronislav
Jarmil Kamil Kristián Lubor Luděk Marcel Mojmír Ota Oskar René Slavomír Vilém Zdenko
"""

_CZECH_SK_FEMALE = """
Jana Marie Eva Hana Anna Lenka Kateřina Lucie Věra Alena Petra Veronika Jaroslava
Tereza Martina Michaela Jitka Helena Ludmila Zdeňka Ivana Monika Eliška Zuzana Markéta
Jarmila Barbora Kristýna Marcela Dagmar Dana Klára Vlasta Natálie Pavla Andrea Irena
Božena Libuše Adéla Šárka Denisa Simona Olga Renata Karolína Romana Iveta Miroslava
Gabriela Milada Nikola Pavlína Kamila Ilona Anežka Sofie Viktorie Ema Dominika
Alžběta Magdalena Daniela Vendula Radka Sabina Nela Julie Laura Amálie Rozálie
Valerie Emilie Johana Vanesa Aneta Miluše Soňa Silvie Zdena Stanislava Vladimíra
Bohumila Drahomíra Taťána Hedvika Růžena Blanka Linda Lada Stela Ivona Jiřina
Kristina Leona Lýdie Marta Milena Naděžda Radmila Regina Sandra Svatava Věroslava
Zlata Žaneta Zita Mária Katarína Zuzka Janka Lucia Ľudmila
"""

_OTHER_FIRST = """
James John Robert Michael William David Richard Joseph Thomas Charles Christopher
Daniel Matthew Anthony Donald Steven Paul Andrew Joshua Kenneth Kevin Brian George
Edward Ronald Timothy Jason Jeffrey Ryan Jacob Gary Nicholas Eric Stephen Jonathan
Larry Justin Scott Brandon Benjamin Samuel Gregory Alexander Patrick Raymond Jack
Dennis Jerry Tyler Aaron Jose Henry Douglas Nathan Peter Zachary Kyle Walter Harold
Jeremy Ethan Carl Keith Roger Gerald Christian Terry Sean Arthur Austin Noah Lawrence
Jesse Joe Bryan Billy Jordan Albert Dylan Bruce Willie Gabriel Alan Juan Logan Wayne
Ralph Roy Eugene Randy Vincent Russell Louis Philip Bobby Johnny Bradley Oliver Harry
Charlie Liam Lucas Mason Elijah Owen Leo Mary Patricia Jennifer Linda Elizabeth
Barbara Susan Jessica Sarah Karen Nancy Lisa Betty Margaret Sandra Ashley Kimberly
Emily Donna Michelle Dorothy Carol Amanda Melissa Deborah Stephanie Rebecca Sharon
Laura Cynthia Kathleen Amy Shirley Angela Helen Brenda Pamela Nicole Emma Samantha
Katherine Christine Debra Rachel Catherine Carolyn Janet Ruth Maria Heather Diane
Virginia Julie Joyce Victoria Olivia Kelly Christina Lauren Joan Evelyn Judith Megan
Cheryl Hannah Martha Jacqueline Frances Gloria Ann Teresa Kathryn Sara Janice Jean
Alice Madison Doris Abigail Julia Judy Grace Denise Amber Marilyn Beverly Danielle
Theresa Sophia Diana Brittany Isabella Charlotte Rose Alexis Kayla Ava Mia Amelia
Harper Chloe Lucy Ella Zoe Lily Hans Klaus Jürgen Wolfgang Stefan Andreas Uwe Sabine
Ursula Claudia Katrin Sven Lukas Leon Felix Maximilian Lena Tobias Florian Matthias
Markus Sebastian Christoph Johannes Heike Birgit Anja Susanne Tomasz Piotr Krzysztof
Andrzej Pawel Marcin Agnieszka Katarzyna Malgorzata Joanna Mark Will Bill Grant May
June April August Frank Pat Max Rich Chase Hope Joy Faith Dean Guy Eve Ray Don Dawn
Gene Jade Ruby Ivy Holly Penny Sandy Glen Miles Wade Drew Earl Dan
"""

# First names that are also ordinary words, months or places. They only count as a
# name when another capitalised word stands next to them ("Mark Twain", not "Mark").
AMBIGUOUS_FIRST = {
    fold(w) for w in """
    Jan Will May Mark Bill Grant Rose June April August Art Frank Pat Max Rich Chase
    Hope Joy Faith Grace Dean Guy Eve Ray Don Carol Dawn Gene Amber Jean Jack Dan Milan
    Blanka Aleš Lane Page Glen Miles Harper Jordan Austin Victoria Virginia Sofie Logan
    Christian Sandy Ruby Jade Ivy Holly Penny Wade Drew Earl Lada Stela Zlata Leona Regina
    Ota Ema Otto Leo Roman Vít
    """.split()
}

# Declined forms that collide with common words (daně = taxes, Anno, vita).
AMBIGUOUS_FORMS = {"dane", "dani", "danu", "anno", "vita", "vite"}

# Academic titles, salutations and honorifics that precede a name.
TITLES = {
    fold(w) for w in """
    Ing Mgr Bc BcA MgA MUDr MDDr MVDr JUDr PhDr RNDr PharmDr PaedDr ThDr ThLic ICDr
    Dr Doc Prof PhD Ph.D CSc DrSc DiS MBA LLM LL.M MSc Dipl
    Mr Mrs Ms Miss Mx Sir Mister Dame Lord Lady
    Pan Pane Paní Pana Panu Panem Panovi Slečna Slečno Slečny Slečnou
    Herr Frau Dear Vážený Vážená Vážení Milý Milá Kolega Kolegyně
    """.split()
}

# Capitalised words that are never part of a name even when they follow one.
NAME_STOPWORDS = {
    fold(w) for w in """
    The A An And Or Of In On At To For From With By Ltd Inc Plc GmbH AG SE Corp Co
    Company Group Street Road Avenue
    Leden Únor Březen Duben Květen Červen Červenec Srpen Září Říjen Listopad Prosinec
    Ledna Února Března Dubna Května Června Července Srpna Října Listopadu Prosince
    January February March July September October November December
    Monday Tuesday Wednesday Thursday Friday Saturday Sunday
    Pondělí Úterý Středa Čtvrtek Pátek Sobota Neděle
    Praha Brno Ostrava Plzeň Olomouc Liberec Bratislava Košice Prague Vienna Wien
    Česká Republika ČR SR EU Kč CZK EUR USD Smlouva Faktura Příloha Strana Page
    Dne Datum Podpis Signature Customer Client Madam Colleague Colleagues Team Kolego
    Kolegové Zákazníku Kliente Všichni All
    """.split()
}

_COMMON_SURNAMES = """
Novák Svoboda Novotný Dvořák Černý Procházka Kučera Veselý Horák Němec Pokorný
Marek Pospíšil Hájek Jelínek Král Růžička Beneš Fiala Sedláček Doležal Zeman Kolář
Navrátil Čermák Vaněk Urban Blažek Kříž Kovář Kratochvíl Bartoš Vlček Polák Musil
Kopecký Šimek Konečný Malý Holub Čech Štěpánek Staněk Kadlec Dostál Soukup Šťastný
Mareš Moravec Sýkora Tichý Valenta Vávra Matoušek Bláha Říha Ševčík Bureš Hruška
Mašek Pavlík Dušek Janda Hrubý Mach Kubíček Hladík Smith Johnson Williams Brown Jones
Garcia Miller Davis Wilson Anderson Taylor Moore Jackson Martin Lee Thompson White
Harris Clark Lewis Walker Hall Young Allen King Wright Scott Green Baker Adams Nelson
Müller Schmidt Schneider Fischer Weber Meyer Wagner Becker Schulz Hoffmann Nowak
Kowalski Wiśniewski Kováč Varga Tóth Nagy Horváth
"""


def _czech_forms(name: str) -> set:
    """Nominative plus the most common declined forms of a Czech/Slovak first name."""
    n = fold(name)
    forms = {n}
    if n.endswith(("a", "e")):
        stem = n[:-1]
        forms |= {stem + s for s in ("y", "e", "u", "ou", "o", "i")}
    elif n.endswith("o"):
        stem = n[:-1]
        forms |= {stem + s for s in ("a", "ovi", "em", "u")}
    elif n.endswith("i"):  # Jiří
        forms |= {n + s for s in ("ho", "mu", "m")}
    else:
        stems = {n}
        if n.endswith("ek"):  # Marek -> Marka
            stems.add(n[:-2] + "k")
        if n.endswith("el"):  # Pavel -> Pavla
            stems.add(n[:-2] + "l")
        for stem in stems:
            forms |= {stem + s for s in ("a", "e", "ovi", "em", "u", "i", "o")}
    return forms


def _build_first_names() -> dict:
    """Map folded form -> True if the form is ambiguous (needs a neighbouring name)."""
    table: dict = {}

    def add(form: str, base: str) -> None:
        ambiguous = fold(base) in AMBIGUOUS_FIRST
        # A form is unambiguous if any name that produces it is unambiguous.
        table[form] = table.get(form, True) and (ambiguous or form in AMBIGUOUS_FORMS)

    for name in (_CZECH_SK_MALE + _CZECH_SK_FEMALE).split():
        for form in _czech_forms(name):
            add(form, name)
    for name in _OTHER_FIRST.split():
        add(fold(name), name)
    return table


FIRST_NAMES = _build_first_names()


def _surname_forms(name: str) -> set:
    n = fold(name)
    forms = {n}
    if n.endswith("y"):  # Černý -> Černého, Černému, Černým
        forms |= {n[:-1] + s for s in ("eho", "emu", "ym")}
    elif n.endswith("a"):  # Svoboda -> Svobody, Svobodovi
        forms |= {n[:-1] + s for s in ("y", "ovi", "ou", "u", "o")}
    else:
        forms |= {n + s for s in ("a", "ovi", "em", "e", "u")}
    return forms


COMMON_SURNAMES = set().union(*(_surname_forms(s) for s in _COMMON_SURNAMES.split()))

FEMALE_SURNAME_ENDINGS = ("ova", "ove", "ovou", "ska", "ske", "skou", "cka", "cke", "ckou")


def first_name_status(token_folded: str):
    """None if not a first name, else True when the name is ambiguous."""
    return FIRST_NAMES.get(token_folded)


def looks_like_surname(token_folded: str) -> bool:
    if token_folded in COMMON_SURNAMES:
        return True
    return len(token_folded) >= 6 and token_folded.endswith(FEMALE_SURNAME_ENDINGS)
