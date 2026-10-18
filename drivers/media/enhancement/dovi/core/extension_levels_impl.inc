/* Extension levels that replace an existing record of the same level. */
static int unique_level(u32 level)
{
    switch (level) {
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 9:
    case 11:
    case 254:
    case 255:
        return 1;
    default:
        return 0;
    }
}
