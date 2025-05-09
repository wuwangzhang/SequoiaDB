    /**
     * Get the matching documents in current collection and continue monitoring for new data.
     * This method enables "tail" mode similar to Unix "tail -f" command, where the cursor
     * will wait for new data that matches the query conditions rather than closing when all
     * existing data has been returned.
     * 
     * Note: Tail mode is only supported for table scans (TBSCAN), not for index scans (IXSCAN).
     * If an index is specified in the hint, tail mode will be ignored.
     *
     * @param matcher  the matching rule, return all the documents if null
     * @param selector the selective rule, return the whole document if null
     * @param orderBy  the ordered rule, never sort if null
     * @param hint     Specified the index used to scan data. For tail mode, it's recommended to use
     *                 {"":null} to force table scan.
     * @return a DBCursor instance of the result or null if no any matched document
     * @throws BaseException If error happens.
     */
    public DBCursor tail(BSONObject matcher, BSONObject selector, BSONObject orderBy,
                         BSONObject hint) throws BaseException {
        return tail(matcher, selector, orderBy, hint, 0, -1);
    }
    
    /**
     * Get the matching documents in current collection and continue monitoring for new data.
     * This method enables "tail" mode similar to Unix "tail -f" command, where the cursor
     * will wait for new data that matches the query conditions rather than closing when all
     * existing data has been returned.
     * 
     * Note: Tail mode is only supported for table scans (TBSCAN), not for index scans (IXSCAN).
     * If an index is specified in the hint, tail mode will be ignored.
     *
     * @param matcher    the matching rule, return all the documents if null
     * @param selector   the selective rule, return the whole document if null
     * @param orderBy    the ordered rule, never sort if null
     * @param hint       Specified the index used to scan data. For tail mode, it's recommended to use
     *                   {"":null} to force table scan.
     * @param skipRows   skip the first numToSkip documents, never skip if this parameter is 0
     * @param returnRows return the specified amount of documents, when returnRows is 0, return nothing,
     *                   when returnRows is -1, return all the documents
     * @return a DBCursor instance of the result or null if no any matched document
     * @throws BaseException If error happens.
     */
    public DBCursor tail(BSONObject matcher, BSONObject selector, BSONObject orderBy,
                         BSONObject hint, long skipRows, long returnRows) throws BaseException {
        int flags = DBQuery.FLG_QUERY_TAIL;
        return query(matcher, selector, orderBy, hint, skipRows, returnRows, flags);
    }
