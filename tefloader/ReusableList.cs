// /*******************************************************************************
//  * tefkernel - ReusableList.cs
//  * Copyright (C) 2025 eternalfuture-e38299
//  *
//  * This program is free software: you can redistribute it and/or modify
//  * it under the terms of the GNU Affero General Public License as published by
//  * the Free Software Foundation, either version 3 of the License, or
//  * (at your option) any later version.
//  *
//  * This program is distributed in the hope that it will be useful,
//  * but WITHOUT ANY WARRANTY; without even the implied warranty of
//  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//  * GNU Affero General Public License for more details.
//  *
//  * You should have received a copy of the GNU Affero General Public License
//  * along with this program. If not, see <https://www.gnu.org/licenses/>.
//  *
//  * Author: eternalfuture-e38299
//  * GitHub: https://github.com/eternalfuture-e38299
//  * Created: 2025/11/23
//  *******************************************************************************/

using System.Collections;

namespace tefloader;

public class ReusableList<T>(int initialCapacity = 256) : IEnumerable<T>
{
    private T[] _items = new T[initialCapacity];
    private int _count;
    private readonly Stack<int> _freeIndices = new();
    private Dictionary<T, int> _valueToIndexMap = new(initialCapacity);
    private readonly EqualityComparer<T> _comparer = EqualityComparer<T>.Default;
    
    // 使用 ReaderWriterLockSlim 实现高效的读写锁
    private readonly ReaderWriterLockSlim _lock = new();

    public int Count 
    {
        get 
        {
            _lock.EnterReadLock();
            try { return _count; }
            finally { _lock.ExitReadLock(); }
        }
    }

    public int Capacity 
    {
        get 
        {
            _lock.EnterReadLock();
            try { return _items.Length; }
            finally { _lock.ExitReadLock(); }
        }
    }

    public bool IsReadOnly => false;

    /// <summary>
    /// 添加元素并返回其索引
    /// </summary>
    public int Add(T item)
    {
        if (item == null)
            throw new ArgumentNullException(nameof(item));

        _lock.EnterWriteLock();
        try
        {
            if (_freeIndices.Count > 0)
            {
                // 优先使用空位
                var index = _freeIndices.Pop();
                _items[index] = item;
                _valueToIndexMap[item] = index;
                _count++;
                return index;
            }
            else
            {
                // 没有空位，添加到末尾
                if (_count == _items.Length)
                {
                    // 需要扩容
                    var newCapacity = _items.Length * 2;
                    Array.Resize(ref _items, newCapacity);
                }
                
                var index = _count;
                _items[index] = item;
                _valueToIndexMap[item] = index;
                _count++;
                return index;
            }
        }
        finally
        {
            _lock.ExitWriteLock();
        }
    }

    public bool Remove(T item)
    {
        if (item == null)
            return false;

        _lock.EnterWriteLock();
        try
        {
            if (_valueToIndexMap.TryGetValue(item, out int index))
            {
                RemoveAtInternal(index);
                return true;
            }
            return false;
        }
        finally
        {
            _lock.ExitWriteLock();
        }
    }

    public void RemoveAt(int index)
    {
        _lock.EnterWriteLock();
        try
        {
            RemoveAtInternal(index);
        }
        finally
        {
            _lock.ExitWriteLock();
        }
    }

    private void RemoveAtInternal(int index)
    {
        if (index < 0 || index >= _items.Length)
            throw new ArgumentOutOfRangeException(nameof(index));

        T item = _items[index];
        if (_comparer.Equals(item, default!))
            return; // 已经是空位

        _items[index] = default!;
        _valueToIndexMap.Remove(item);
        _freeIndices.Push(index);
        _count--;
    }

    public int IndexOf(T item)
    {
        if (item == null)
            return -1;

        _lock.EnterReadLock();
        try
        {
            return _valueToIndexMap.TryGetValue(item, out int index) ? index : -1;
        }
        finally
        {
            _lock.ExitReadLock();
        }
    }

    public bool Contains(T item)
    {
        if (item == null)
            return false;

        _lock.EnterReadLock();
        try
        {
            return _valueToIndexMap.ContainsKey(item);
        }
        finally
        {
            _lock.ExitReadLock();
        }
    }

    public void Clear()
    {
        _lock.EnterWriteLock();
        try
        {
            Array.Clear(_items, 0, _items.Length);
            _freeIndices.Clear();
            _valueToIndexMap.Clear();
            _count = 0;
        }
        finally
        {
            _lock.ExitWriteLock();
        }
    }

    public void CopyTo(T[] array, int arrayIndex)
    {
        if (array == null)
            throw new ArgumentNullException(nameof(array));
        if (arrayIndex < 0 || arrayIndex >= array.Length)
            throw new ArgumentOutOfRangeException(nameof(arrayIndex));
        if (array.Length - arrayIndex < _count)
            throw new ArgumentException("目标数组空间不足");

        _lock.EnterReadLock();
        try
        {
            var destIndex = arrayIndex;
            foreach (var t in _items)
            {
                if (!_comparer.Equals(t, default!))
                {
                    array[destIndex++] = t;
                }
            }
        }
        finally
        {
            _lock.ExitReadLock();
        }
    }

    public T this[int index]
    {
        get
        {
            _lock.EnterReadLock();
            try
            {
                if (index < 0 || index >= _items.Length)
                    throw new ArgumentOutOfRangeException(nameof(index));
                return _items[index];
            }
            finally
            {
                _lock.ExitReadLock();
            }
        }
        set
        {
            _lock.EnterWriteLock();
            try
            {
                if (index < 0 || index >= _items.Length)
                    throw new ArgumentOutOfRangeException(nameof(index));
                
                var oldValue = _items[index];
                var wasEmpty = _comparer.Equals(oldValue, default!);
                
                // 如果旧值存在，从字典中移除
                if (!wasEmpty)
                {
                    _valueToIndexMap.Remove(oldValue);
                    _count--;
                }
                
                _items[index] = value;
                
                // 如果新值不是默认值，添加到字典
                if (!_comparer.Equals(value, default!))
                {
                    _valueToIndexMap[value] = index;
                    _count++;
                    
                    // 如果这个索引在空闲列表中，移除它
                    if (_freeIndices.Contains(index))
                    {
                        var tempStack = new Stack<int>();
                        while (_freeIndices.Count > 0)
                        {
                            int freeIndex = _freeIndices.Pop();
                            if (freeIndex != index)
                                tempStack.Push(freeIndex);
                        }
                        while (tempStack.Count > 0)
                        {
                            _freeIndices.Push(tempStack.Pop());
                        }
                    }
                }
                else
                {
                    // 新值是默认值，添加到空闲列表
                    _freeIndices.Push(index);
                }
            }
            finally
            {
                _lock.ExitWriteLock();
            }
        }
    }

    public IEnumerator<T> GetEnumerator()
    {
        _lock.EnterReadLock();
        try
        {
            foreach (var t in _items)
            {
                if (!_comparer.Equals(t, default!))
                {
                    yield return t;
                }
            }
        }
        finally
        {
            _lock.ExitReadLock();
        }
    }

    IEnumerator IEnumerable.GetEnumerator()
    {
        return GetEnumerator();
    }

    /// <summary>
    /// 获取实际使用的存储数组（包含空位）
    /// </summary>
    public T[] GetInternalArray()
    {
        _lock.EnterReadLock();
        try
        {
            return (T[])_items.Clone();
        }
        finally
        {
            _lock.ExitReadLock();
        }
    }

    /// <summary>
    /// 获取所有非空元素的数组
    /// </summary>
    public T[] ToArray()
    {
        _lock.EnterReadLock();
        try
        {
            T[] result = new T[_count];
            int index = 0;
            foreach (var item in this)
            {
                result[index++] = item;
            }
            return result;
        }
        finally
        {
            _lock.ExitReadLock();
        }
    }

    /// <summary>
    /// 压缩数组，移除所有空位
    /// </summary>
    public void Compact()
    {
        _lock.EnterWriteLock();
        try
        {
            var newArray = new T[_count];
            var newMap = new Dictionary<T, int>(_count);
            
            var newIndex = 0;
            foreach (var item in this)
            {
                newArray[newIndex] = item;
                newMap[item] = newIndex;
                newIndex++;
            }
            
            _items = newArray;
            _valueToIndexMap = newMap;
            _freeIndices.Clear();
        }
        finally
        {
            _lock.ExitWriteLock();
        }
    }

    ~ReusableList()
    {
        _lock.Dispose();
    }
}