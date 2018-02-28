// Returns a pointer to a buffer
var c_board_create        = Module.cwrap('Board_create',           'number', ['number', 'number'])
var c_board_initialize    = Module.cwrap('Board_maxify',           'number', ['number'])
var c_board_import_array  = Module.cwrap('Board_import_array',         null, ['array'])
var c_get_full_tile       = Module.cwrap('Board_get_full_tile',    'number', ['number', 'number'])
var c_get_reduced_tile    = Module.cwrap('Board_get_reduced_tile', 'number', ['number', 'number'])
var c_destroy_buffer      = Module.cwrap('Board_destroy',              null, ['number'])

function getExportValue(c_buffer, index)
{
        return c_get_export_value(c_buffer, index)
}

function generateFullBoard(width, height, maxValue)
{
        height = height || width
        maxValue = maxValue || Math.min(width, height)
        maxValue = Math.min(maxValue, 9)

        var length = width * height
        var buffer = c_generate_full_board(width, height, maxValue)

        var exportValues = new Array(length)

        for (var i = 0; i < length; i++) {
                exportValues[i] = getExportValue(buffer, i)
        }

        return exportValues
}

function reduceFullBoard(width, height, exportValues)
{
        var length = width * height

        var arr = new Int8Array(length)
        for (var i = 0; i < length; i++) {
                arr[i] = exportValues[i]
        }

        var buffer = c_reduce_full_board(width, height, arr)

        var ret = new Array(length)
        for (var i = 0; i < length; i++) {
                ret[i] = getExportValue(buffer, i)
        }
        return ret
}
