// Golden parity runner (C# side): reads a scenario JSON from test_data/parity/scenarios,
// generates a layout with the reference Edgar-DotNet DungeonGenerator and dumps the logical
// structure (room outlines/positions/doors) as JSON for comparison with the C++ port.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Edgar.Geometry;
using Edgar.Legacy.Core.Doors.SimpleMode;
using Edgar.Legacy.Core.LayoutGenerators.DungeonGenerator;
using Edgar.Legacy.Core.MapDescriptions;
using Edgar.Legacy.Core.MapDescriptions.Interfaces;
using Edgar.Legacy.GeneralAlgorithms.DataStructures.Common;
using Edgar.Legacy.GeneralAlgorithms.DataStructures.Polygons;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace LevelSynth.ParityRunner
{
    public static class Program
    {
        public static int Main(string[] args)
        {
            if (args.Length < 2)
            {
                Console.Error.WriteLine("usage: parity_runner_cs <scenario.json> <output.json>");
                return 2;
            }

            var scenario = JObject.Parse(File.ReadAllText(args[0]));
            var name = scenario.Value<string>("name");
            var seed = scenario.Value<int>("seed");

            var basicTemplates = LoadTemplates(scenario["templates"]);
            var corridorTemplates = LoadTemplates(scenario["corridor_templates"]);

            var mapDescription = new MapDescription<int>();
            foreach (var room in scenario["rooms"])
            {
                var id = room.Value<int>("id");
                var isCorridor = room.Value<bool?>("corridor") ?? false;
                var templates = isCorridor ? corridorTemplates : basicTemplates;
                if (templates.Count == 0)
                    throw new InvalidOperationException($"No templates for room {id} (corridor={isCorridor})");
                IRoomDescription description = isCorridor
                    ? (IRoomDescription)new CorridorRoomDescription(templates)
                    : new BasicRoomDescription(templates);
                mapDescription.AddRoom(id, description);
            }
            foreach (var connection in scenario["connections"])
            {
                mapDescription.AddConnection(connection[0].Value<int>(), connection[1].Value<int>());
            }

            var generator = new DungeonGenerator<int>(mapDescription);
            generator.InjectRandomGenerator(new Random(seed));
            var layout = generator.GenerateLayout();
            if (layout == null)
            {
                Console.Error.WriteLine("generation returned null layout");
                return 1;
            }

            var output = new JObject
            {
                ["scenario"] = name,
                ["seed"] = seed,
                ["engine"] = "csharp",
                ["rooms"] = new JArray(layout.Rooms.Select(room => new JObject
                {
                    ["id"] = room.Node,
                    ["is_corridor"] = room.IsCorridor,
                    ["position"] = new JArray(room.Position.X, room.Position.Y),
                    ["outline"] = new JArray(room.Shape.GetPoints()
                        .Select(p => new JArray(p.X + room.Position.X, p.Y + room.Position.Y))),
                    ["doors"] = new JArray((room.Doors ?? new List<Edgar.GraphBasedGenerator.Grid2D.LayoutDoorGrid2D<int>>())
                        .Select(d => new JObject
                        {
                            ["from"] = d.FromRoom,
                            ["to"] = d.ToRoom,
                            ["line"] = new JArray(
                                new JArray(d.DoorLine.From.X, d.DoorLine.From.Y),
                                new JArray(d.DoorLine.To.X, d.DoorLine.To.Y)),
                        })),
                })),
            };

            File.WriteAllText(args[1], output.ToString(Formatting.Indented));
            Console.WriteLine($"wrote {args[1]} ({layout.Rooms.Count} rooms)");
            return 0;
        }

        private static List<RoomTemplate> LoadTemplates(JToken templatesToken)
        {
            var result = new List<RoomTemplate>();
            if (templatesToken == null)
            {
                return result;
            }

            foreach (var t in templatesToken)
            {
                var rect = t["rect"];
                var shape = PolygonGrid2D.GetRectangle(rect[0].Value<int>(), rect[1].Value<int>());
                var doorMode = new SimpleDoorMode(t.Value<int>("door_length"), t.Value<int>("corner_distance"));
                result.Add(new RoomTemplate(shape, doorMode,
                    TransformationGrid2DHelper.GetAllTransformationsOld().ToList()));
            }
            return result;
        }
    }
}
