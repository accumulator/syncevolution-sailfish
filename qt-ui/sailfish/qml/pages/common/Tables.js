.pragma library

var syncModes = ["disabled", "two-way", "slow",
                 "refresh-from-client", "refresh-from-server",
                 "one-way-from-client", "one-way-from-server"]

function indexOfSyncMode(mode)
{
    return syncModes.indexOf(mode)
}

function syncModeOfIndex(idx)
{
    return syncModes[idx]
}
