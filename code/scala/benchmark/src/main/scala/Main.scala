import java.nio.file.{Files, Paths}
import java.nio.{ByteBuffer, ByteOrder}
import scala.collection.concurrent.TrieMap
import scala.collection.mutable.ListBuffer

class MyInt (val x: Int){
    override def hashCode() = {x/10}
    override def equals(o: Any) = o match {
        case that : MyInt => (x == that.x)
        case _ => false
    }
    override def toString() = x.toString
}

object Constants {
    val INSERT_TYPE = 0
    val LOOKUP_TYPE = 1
    val REMOVE_TYPE = 2
}

object Arrays {
    var inserts = Array[Op[MyInt, Int]]()
    var removes = Array[Op[MyInt, Int]]()
    var lookups = Array[Op[MyInt, Int]]()
    var actions = Array[Op[MyInt, Int]]()
}

trait Op[Key, Value] {
    def execute(m: TrieMap[Key, Value])
}

class InsertOp[Key, Value] (key:Key, value:Value) extends Op[Key, Value]{
    override def execute(m: TrieMap[Key, Value]) {
        //println("inserting " +key.toString +":"+value)
        m(key) = value
    }
}

class LookupOp[Key, Value] (key:Key, pad:Value = 0) extends Op[Key, Value] {
    override def execute(m: TrieMap[Key, Value]) {
        try {
            val value = m(key)
            //println("got " + value + " for " + key)
        } 
        catch {
        case e : Exception => println("got exception for " + key)
        }
    }
}

class RemoveOp[Key, Value] (key:Key, pad:Value = 0) extends Op[Key, Value] {
    override def execute(m: TrieMap[Key, Value]) {
        try {
            m.remove(key)
            //println("removed " + key)
        } 
        catch {
        case e : Exception => println("got exception for " + key)
        }
    }
}

class WorkerThread[Key, Value](m: TrieMap[Key, Value], ops: Array[Op[Key, Value]], start: Int, end: Int) extends Runnable {
    def run() {
        for (i <- start until end) {
            ops(i).execute(m)
        }
    }
}


object Main {

    def parseInsert(path: String) = {
        // Read the data and wrap it with a little endian buffer
        println("Reading data from" + path)
        val data = Files.readAllBytes(Paths.get(path))
        val wrapper = ByteBuffer.wrap(data)
        wrapper.order(ByteOrder.LITTLE_ENDIAN);
        val size = wrapper.getInt
        Arrays.inserts = new Array[Op[MyInt, Int]](size)
        for (i <- 0 until size) {
            val key = new MyInt(wrapper.getInt)
            val value = wrapper.getInt
            Arrays.inserts(i) = new InsertOp[MyInt, Int](key, value)
        }
   }

    def handleInsert (numOfThreads: Int, map: TrieMap[MyInt, Int]) = {
        val chunk = Arrays.inserts.length / numOfThreads
        val threads = new ListBuffer[Thread]()
        for (i <- 1 to numOfThreads) {
            threads +=  new Thread(new WorkerThread[MyInt, Int](map, Arrays.inserts, (i-1)*chunk, i*chunk))
        }
        val start_time = System.nanoTime()
        threads.foreach(t => t.start())
        threads.foreach(t => t.join())
        val end_time = System.nanoTime()
        println("Done insert in " + (end_time - start_time) + " nsec");
    }

    def parseLookup(path: String) = {
        // Read the data and wrap it with a little endian buffer
        println("Reading data from" + path)
        val data = Files.readAllBytes(Paths.get(path))
        val wrapper = ByteBuffer.wrap(data)
        wrapper.order(ByteOrder.LITTLE_ENDIAN);
        val size = wrapper.getInt
        Arrays.lookups = new Array[Op[MyInt, Int]](size)
        for (i <- 0 until size) {
            val key = new MyInt(wrapper.getInt)
            Arrays.lookups(i) = new LookupOp[MyInt, Int](key)
        }
    }

    def handleLookup (numOfThreads: Int, map: TrieMap[MyInt, Int]) = {
        val chunk = Arrays.lookups.length / numOfThreads
        val threads = new ListBuffer[Thread]()
        for (i <- 1 to numOfThreads) {
            threads +=  new Thread(new WorkerThread[MyInt, Int](map, Arrays.lookups, (i-1)*chunk, i*chunk))
        }
        val start_time = System.nanoTime()
        threads.foreach(t => t.start())
        threads.foreach(t => t.join())
        val end_time = System.nanoTime()
        println("Done lookup in " + (end_time - start_time) + " nsec");
    }

    def parseRemove(path: String) = {
        // Read the data and wrap it with a little endian buffer
        println("Reading data from" + path)
        val data = Files.readAllBytes(Paths.get(path))
        val wrapper = ByteBuffer.wrap(data)
        wrapper.order(ByteOrder.LITTLE_ENDIAN);
        val size = wrapper.getInt
        Arrays.removes = new Array[Op[MyInt, Int]](size)
        for (i <- 0 until size) {
            val key = new MyInt(wrapper.getInt)
            Arrays.removes(i) = new RemoveOp[MyInt, Int](key)
        }
    }

    def handleRemove (numOfThreads: Int, map: TrieMap[MyInt, Int]) = {
        // Read the data and wrap it with a little endian buffer
        val chunk = Arrays.removes.length / numOfThreads
        val threads = new ListBuffer[Thread]()
        for (i <- 1 to numOfThreads) {
            threads +=  new Thread(new WorkerThread[MyInt, Int](map, Arrays.removes, (i-1)*chunk, i*chunk))
        }
        val start_time = System.nanoTime()
        threads.foreach(t => t.start())
        threads.foreach(t => t.join())
        val end_time = System.nanoTime()
        println("Done remove in " + (end_time - start_time) + " nsec");
    }

    def parseGeneric(path: String) = {
        // Read the data and wrap it with a little endian buffer
        println("Reading data from" + path)
        val data = Files.readAllBytes(Paths.get(path))
        val wrapper = ByteBuffer.wrap(data)
        wrapper.order(ByteOrder.LITTLE_ENDIAN);
        val size = wrapper.getInt
        Arrays.actions = new Array[Op[MyInt, Int]](size)
        for (i <- 0 until size) {
            val key = new MyInt(wrapper.getInt)
            Arrays.actions(i) = wrapper.getInt match {
                case Constants.INSERT_TYPE => new InsertOp[MyInt, Int](new MyInt(wrapper.getInt), wrapper.getInt)
                case Constants.LOOKUP_TYPE => new LookupOp[MyInt, Int](new MyInt(wrapper.getInt), wrapper.getInt)
                case Constants.REMOVE_TYPE => new RemoveOp[MyInt, Int](new MyInt(wrapper.getInt), wrapper.getInt)
            }
        }
    }

    def handleGeneric (numOfThreads: Int, map: TrieMap[MyInt, Int]) = {
        val chunk = Arrays.actions.length / numOfThreads
        val threads = new ListBuffer[Thread]()
        for (i <- 1 to numOfThreads) {
            threads +=  new Thread(new WorkerThread[MyInt, Int](map, Arrays.actions, (i-1)*chunk, i*chunk))
        }
        val start_time = System.nanoTime()
        threads.foreach(t => t.start())
        threads.foreach(t => t.join())
        val end_time = System.nanoTime()
        println("Done generic action in " + (end_time - start_time) + " nsec");
    }

    def iteration(numOfThreads: Int, args: Array[String]) = {
        val map = new TrieMap[MyInt, Int]()

        for (i <- 2 to args.length - 1 by 2) {
            args(i) match {
            case "insert" => handleInsert(numOfThreads, map)
            case "lookup" => handleLookup(numOfThreads, map)
            case "remove" => handleRemove(numOfThreads, map)
            case "action" => handleGeneric(numOfThreads, map)
            }
        }
    }

    def main(args: Array[String]) = {
        if (args.length < 4 || args.length % 2 == 1) {
            println("USAGE: run <num_of_threads> <iterations> <action> <file> [<action> <file>]...")
        }
        else {
            for (i <- 2 to args.length -1 by 2) {
                args(i) match {
                case "insert" => parseInsert(args(i+1))
                case "lookup" => parseLookup(args(i+1))
                case "remove" => parseRemove(args(i+1))
                case "action" => parseGeneric(args(i+1))
                }
            }
            args(0).split(",").map(_.toInt).foreach(num =>
                for (i <- 1 to args(1).toInt) {
                    println(num + " threads: iteration number " + i)
                    Runtime.getRuntime.gc()
                    iteration(num, args)
                }
            )
        }
    }
}
