// ── MultiplayerStartGen ──────────────────────────────────────────────────────
// Genera KenshiCoop-MultiplayerStart.mod: un Game Start "Multiplayer (Wanderer x2)"
// idéntico al inicio "Wanderer" vanilla de Kenshi pero con DOS vagabundos, cada uno
// en su PROPIA escuadra (escuadra 1 = host, escuadra 2 = jugador que se une), para
// que KenshiCoop pueda repartir el control por pestaña de escuadra sin pasos manuales
// (README de KenshiCoop, "Good to know": host controla squad 1, el que se une squad 2).
//
// PRINCIPIO DE DISEÑO (regla dura de Zero): 100% vanilla-equivalente.
//   - La escuadra 1 REUTILIZA el SquadTemplate vanilla del Wanderer (45550-gamedata.base)
//     sin tocarlo: es vanilla por construcción.
//   - La escuadra 2 es un SquadTemplate NUEVO cuyo contenido es una copia exacta del
//     efectivo vanilla (valores de gamedata.base + override de rebirth.mod), con un
//     líder NUEVO que es un clon exacto del Character "Wanderer" (1533662-rebirth.mod).
//   - CERO cambios de facción, comportamiento, IA o combate. No se replica ningún
//     "fix" de otros proyectos. Solo se clonan: 1 Character, 1 SquadTemplate, y se
//     crea 1 NewGameStartoff nuevo.
//
// DATOS VANILLA VERIFICADOS (volcados con OpenConstructionSet el 2026-07-19):
//   - NewGameStartoff "Wanderer" (1980-gamedata.base): base define 7 valores
//     (difficulty=Default, money=1000, start pos 4000/4000, style=RPG, ...);
//     rebirth.mod lo sobreescribe con nueva description y añade las refs
//     squad -> 45550-gamedata.base y town -> The Hub (18919-Newwworld.mod).
//   - SquadTemplate "startoff- Wanderer squad" (45550-gamedata.base): base define
//     32 valores; rebirth.mod añade "dont multiply=True" y leader -> 1533662-rebirth.mod.
//   - Character "Wanderer" (1533662-rebirth.mod): registro NUEVO completo en rebirth.mod
//     (30 valores + 11 referencias: ropa, arma Iron Club, dialogue package, etc.).
//   - La categoría 'squad' de un game start admite MÚLTIPLES referencias: el propio
//     gamedata.base original lo demuestra ("The Cannibal Hunters" tenía 2 entradas).
//
// USO:
//   dotnet run -c Release            # genera y verifica (releyendo el fichero escrito)
//   dotnet run -c Release -- dump    # solo vuelca el .mod ya generado (verificación manual)
using OpenConstructionSet.Data;
using OpenConstructionSet.Mods;

// ── Rutas ────────────────────────────────────────────────────────────────────
const string KenshiData = @"E:\SteamLibrary\steamapps\common\Kenshi\data";
const string ModName = "KenshiCoop-MultiplayerStart";
// Salida siguiendo el patrón dist/mods/<Nombre>/<Nombre>.mod del repo (igual que KenshiCoop/).
// La carpeta con el mismo nombre que el .mod permite copiarla tal cual a Kenshi\mods\.
string outDir = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, @"..\..\..\..\..\dist\mods", ModName));
string outPath = Path.Combine(outDir, ModName + ".mod");

// StringIds vanilla que usamos (verificados en los volcados de arriba)
const string VanillaStartId = "1980-gamedata.base";        // game start "Wanderer"
const string VanillaSquadId = "45550-gamedata.base";       // squad template "startoff- Wanderer squad"
const string VanillaCharId  = "1533662-rebirth.mod";       // character "Wanderer"
const string HubTownId      = "18919-Newwworld.mod";       // town "The Hub" (spawn vanilla del Wanderer)

// ── Modo dump: volcar el .mod generado para verificación manual ──────────────
if (args.Length > 0 && args[0] == "dump")
{
    if (!File.Exists(outPath)) { Console.WriteLine($"No existe {outPath} — genera primero."); return 1; }
    var d = await new ModFile(outPath).ReadDataAsync();
    DumpAll(d, outPath);
    return 0;
}

// ── 1) Leer los datos base del juego ────────────────────────────────────────
Console.WriteLine("Leyendo datos base de Kenshi...");
var baseData = await new ModFile(Path.Combine(KenshiData, "gamedata.base")).ReadDataAsync();
var rebirth  = await new ModFile(Path.Combine(KenshiData, "rebirth.mod")).ReadDataAsync();
Console.WriteLine($"  gamedata.base: {baseData.Items.Count} items | rebirth.mod: {rebirth.Items.Count} items");

// Búsqueda por StringId en un fichero concreto (tolerante a duplicados: gana el último, como en el juego)
static Item? Find(IEnumerable<Item> items, string stringId) =>
    items.LastOrDefault(i => i.StringId.Equals(stringId, StringComparison.OrdinalIgnoreCase));

var startBase = Find(baseData.Items, VanillaStartId);
var startOver = Find(rebirth.Items,  VanillaStartId);
var squadBase = Find(baseData.Items, VanillaSquadId);
var squadOver = Find(rebirth.Items,  VanillaSquadId);
var wandChar  = Find(rebirth.Items,  VanillaCharId);

// Validación dura: si el juego cambia y estos registros no están, abortar con claridad.
if (startBase is null || startOver is null || squadBase is null || squadOver is null || wandChar is null)
{
    Console.WriteLine("ERROR: no se encontraron los registros vanilla esperados:");
    Console.WriteLine($"  start base={startBase is not null} startOver={startOver is not null} squadBase={squadBase is not null} squadOver={squadOver is not null} char={wandChar is not null}");
    return 1;
}
Console.WriteLine($"  Vanilla OK: start \"{startOver.Name}\", squad \"{squadBase.Name}\", char \"{wandChar.Name}\"");

// ── 2) Valores EFECTIVOS vanilla = base + override de rebirth ───────────────
// Kenshi aplica los .mod como deltas: un registro "Changed" sobreescribe los valores
// que lista y añade sus referencias. Reproducimos esa fusión para copiar el resultado real.
static Dictionary<string, object> MergeValues(Item baseItem, Item overrideItem)
{
    var merged = new Dictionary<string, object>(baseItem.Values);
    foreach (var kv in overrideItem.Values) merged[kv.Key] = kv.Value; // el override gana
    return merged;
}
var startValues = MergeValues(startBase, startOver);   // 7 valores con la description de rebirth
var squadValues = MergeValues(squadBase, squadOver);   // 32 valores con dont multiply=True

// ── 3) Construir los 3 items nuevos del mod ─────────────────────────────────
// IDs locales 1..3; StringId convención FCS: "{id}-{nombre del .mod}"
string Sid(int id) => $"{id}-{ModName}.mod";
var newSave = new ItemSaveData(1, ItemChangeType.New); // registro NUEVO (igual que los items nuevos de rebirth.mod)

// 3a) Clon EXACTO del Character "Wanderer" vanilla -> "Wanderer 2" (líder de la escuadra 2).
//     Deep-copy de valores y categorías de referencia (ropa, arma, diálogo, personalidad...).
//     Nada más se cambia: mismo equipo, mismos stats, misma facción implícita que el original.
var char2 = new Item(
    ItemType.Character, 1, "Wanderer 2", Sid(1), newSave,
    new Dictionary<string, object>(wandChar.Values),
    wandChar.ReferenceCategories.Select(c => new ReferenceCategory(c)),
    Enumerable.Empty<Instance>());

// 3b) SquadTemplate nuevo para la escuadra 2: copia exacta del efectivo vanilla del
//     "startoff- Wanderer squad", con el líder apuntando al clon (v0=1 = 1 unidad,
//     mismo valor que usa el vanilla en su ref 'leader').
var squad2 = new Item(
    ItemType.SquadTemplate, 2, "startoff- Wanderer squad 2 (co-op)", Sid(2), newSave,
    squadValues,
    new[] { new ReferenceCategory("leader", new[] { new Reference(Sid(1), 1, 0, 0) }) },
    Enumerable.Empty<Instance>());

// 3c) Game start nuevo: valores efectivos del Wanderer vanilla (dificultad, dinero=1000,
//     posición, estilo... intactos) + description propia para identificarlo en el menú.
//     Referencias: DOS escuadras (1ª = template vanilla intacto, 2ª = la nuestra) y el
//     mismo pueblo de inicio (The Hub) que el Wanderer vanilla.
startValues["description"] =
    "Two lone wanderers with nothing but a few coins, a pair of pants each and a couple of rusty " +
    "swords, ready to venture out into the world together.  Designed for KenshiCoop: each wanderer " +
    "starts in their own squad, so the host controls squad 1 and the joining player controls squad 2.";
var start2 = new Item(
    ItemType.NewGameStartoff, 3, "Multiplayer (Wanderer x2)", Sid(3), newSave,
    startValues,
    new[]
    {
        // ORDEN IMPORTANTE: la primera ref 'squad' debe ser la escuadra del host (vanilla).
        new ReferenceCategory("squad", new[]
        {
            new Reference(VanillaSquadId, 0, 0, 0), // escuadra 1: template vanilla, sin tocar
            new Reference(Sid(2),         0, 0, 0), // escuadra 2: nuestro clon con Wanderer 2
        }),
        new ReferenceCategory("town", new[] { new Reference(HubTownId, 0, 0, 0) }),
    },
    Enumerable.Empty<Instance>());

// ── 4) Ensamblar y escribir el .mod ─────────────────────────────────────────
// Header estilo FCS: dependencias = ficheros base cuyos registros referenciamos
// (mismo patrón observado en mods reales creados con FCS).
var header = new Header(1, "",
    "Multiplayer (Wanderer x2) — a game start for KenshiCoop. Identical to the vanilla Wanderer " +
    "start but with two wanderers, each already in their own squad: the host plays squad 1 and " +
    "the joining player takes squad 2. Data-only mod; requires the KenshiCoop plugin for co-op.")
{
    Dependencies = new List<string> { "gamedata.base", "Newwworld.mod", "rebirth.mod", "Dialogue.mod" },
};
// ModInfoData rellena el _<nombre>.info que usa el launcher (nombre de fichero + título visible)
var info = new ModInfoData { ModName = ModName + ".mod", Title = "Multiplayer (Wanderer x2)", Tags = new[] { "Gameplay" } };
var modData = new ModFileData(DataFileType.Mod, header, 3, new[] { char2, squad2, start2 }, info);

Directory.CreateDirectory(outDir);
await new ModFile(outPath).WriteDataAsync(modData);
Console.WriteLine($"\nEscrito: {outPath} ({new FileInfo(outPath).Length} bytes)");

// ── 5) VERIFICACIÓN por relectura fiel del fichero escrito ──────────────────
var verify = await new ModFile(outPath).ReadDataAsync();
DumpAll(verify, outPath);

// Asserts estructurales: si algo no cuadra, salida con error.
var errors = new List<string>();
if (verify.Items.Count != 3) errors.Add($"esperados 3 items, hay {verify.Items.Count}");

var vChar = verify.Items.FirstOrDefault(i => i.Type == ItemType.Character);
var vSquad = verify.Items.FirstOrDefault(i => i.Type == ItemType.SquadTemplate);
var vStart = verify.Items.FirstOrDefault(i => i.Type == ItemType.NewGameStartoff);
if (vChar is null) errors.Add("falta el Character clonado");
if (vSquad is null) errors.Add("falta el SquadTemplate de la escuadra 2");
if (vStart is null) errors.Add("falta el NewGameStartoff");

if (vChar is not null)
{
    // El clon debe conservar TODOS los valores y refs del Wanderer vanilla
    if (vChar.Values.Count != wandChar.Values.Count)
        errors.Add($"char clon: {vChar.Values.Count} valores (vanilla tiene {wandChar.Values.Count})");
    int vRefs = vChar.ReferenceCategories.Sum(c => c.References.Count);
    int wRefs = wandChar.ReferenceCategories.Sum(c => c.References.Count);
    if (vRefs != wRefs) errors.Add($"char clon: {vRefs} refs (vanilla tiene {wRefs})");
}
if (vSquad is not null)
{
    if (vSquad.Values.Count != squadValues.Count)
        errors.Add($"squad2: {vSquad.Values.Count} valores (esperados {squadValues.Count})");
    var leader = vSquad.ReferenceCategories.FirstOrDefault(c => c.Name == "leader")?.References.SingleOrDefault();
    if (leader is null || leader.TargetId != Sid(1) || leader.Value0 != 1)
        errors.Add("squad2: ref 'leader' no apunta al clon con v0=1");
    if (!Equals(vSquad.Values.TryGetValue("dont multiply", out var dm) ? dm : null, true))
        errors.Add("squad2: falta 'dont multiply'=True (valor efectivo vanilla)");
}
if (vStart is not null)
{
    var squads = vStart.ReferenceCategories.FirstOrDefault(c => c.Name == "squad")?.References ?? new List<Reference>();
    if (squads.Count != 2) errors.Add($"start: {squads.Count} refs 'squad' (esperadas 2)");
    else
    {
        if (squads[0].TargetId != VanillaSquadId) errors.Add("start: la 1ª escuadra no es el template vanilla del host");
        if (squads[1].TargetId != Sid(2)) errors.Add("start: la 2ª escuadra no es nuestro template");
    }
    var town = vStart.ReferenceCategories.FirstOrDefault(c => c.Name == "town")?.References.SingleOrDefault();
    if (town is null || town.TargetId != HubTownId) errors.Add("start: 'town' no es The Hub vanilla");
    // Valores de juego idénticos al Wanderer vanilla (salvo description)
    foreach (var key in new[] { "difficulty", "money", "start pos X", "start pos Z", "style", "force start pos" })
        if (!Equals(vStart.Values.TryGetValue(key, out var v) ? v : null, startBase.Values[key]))
            errors.Add($"start: valor '{key}' difiere del Wanderer vanilla");
}

Console.WriteLine();
if (errors.Count > 0)
{
    Console.WriteLine("VERIFICACIÓN FALLIDA:");
    foreach (var e in errors) Console.WriteLine($"  - {e}");
    return 1;
}
Console.WriteLine("VERIFICACIÓN OK: 3 items bien formados, 2 escuadras cableadas, valores = Wanderer vanilla.");
return 0;

// ── Volcado completo de un .mod (para verificación visual) ──────────────────
static void DumpAll(ModFileData d, string path)
{
    Console.WriteLine($"\n=== DUMP {Path.GetFileName(path)}: {d.Items.Count} items, lastId={d.LastId} ===");
    Console.WriteLine($"HEADER deps=[{string.Join("; ", d.Header.Dependencies)}]");
    foreach (var it in d.Items)
    {
        Console.WriteLine($"\n[{it.Type}] \"{it.Name}\" ({it.StringId}) save={it.SaveData}");
        foreach (var kv in it.Values.OrderBy(k => k.Key))
        {
            var s = kv.Value?.ToString() ?? "";
            Console.WriteLine($"    val {kv.Key} = {(s.Length > 90 ? s[..90] + "..." : s)}");
        }
        foreach (var cat in it.ReferenceCategories)
            foreach (var r in cat.References)
                Console.WriteLine($"    ref {cat.Name} -> {r.TargetId} [v0={r.Value0} v1={r.Value1} v2={r.Value2}]");
    }
}
