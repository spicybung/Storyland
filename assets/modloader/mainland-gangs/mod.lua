storyland.mod {
    name = "Mainland Cholo and Haitian territories",
    priority = 100,
    enabled = true
}

local districts = {
    { id = "little_haiti", name = "Little Haiti", zone = "HAITI", columns = 3, income = 175 },
    { id = "little_havana", name = "Little Havana", zone = "HAVANA", columns = 3, income = 200 },
    { id = "downtown", name = "Downtown", zone = "DTOWN", columns = 2, income = 225 },
    { id = "viceport", name = "Viceport", zone = "DOCKS", columns = 2, income = 250 }
}

local function owner(district, row, column)
    if district == "little_haiti" then
        return (column == 0 or (column == 1 and row == 0)) and "cholos" or "haitians"
    elseif district == "little_havana" then
        return (column == 2 and row == 1) and "haitians" or "cholos"
    elseif district == "downtown" then
        return (column == 1 and row == 1) and "cholos" or "haitians"
    end
    return (column == 0 and row == 0) and "haitians" or "cholos"
end

local territories = {}
for _, district in ipairs(districts) do
    for row = 0, 1 do
        for column = 0, district.columns - 1 do
            territories[#territories + 1] = {
                id = string.format("%s_%d_%d", district.id, row, column),
                name = string.format("%s Turf %d", district.name, row * district.columns + column + 1),
                district_zone = district.zone,
                owner = owner(district.id, row, column),
                bounds = { column / district.columns, row / 2,
                    (column + 1) / district.columns, (row + 1) / 2 },
                daily_income = district.income
            }
        end
    end
end

storyland.gang_turfs {
    population_kind = 10,
    pedestrian_type = 32,
    models = { "BMYCR", "BMYBB" },
    health = 100,
    armour = 0,
    weapon_slots = { 3, 5 },
    excluded_zones = { "AIRPORT", "TERMINL", "BAGGAG", "ARMYBAS", "STARI" },
    territories = territories
}
