/**
 * data_generator.js
 * 
 * Generates test data for SequoiaDB query optimization benchmarks
 */

const config = require('./config');

const args = process.argv.slice(2);
const sizeArg = args.find(arg => arg.startsWith('--size='));
const size = sizeArg ? sizeArg.split('=')[1] : 'small';

if (!['small', 'medium', 'large'].includes(size)) {
    console.error('Invalid size. Must be one of: small, medium, large');
    process.exit(1);
}

console.log(`Generating ${size} dataset...`);

function connectToSequoiaDB() {
    try {
        const conn = new SdbConnection();
        conn.connect(config.connection.host, config.connection.port);
        return conn;
    } catch (err) {
        console.error('Failed to connect to SequoiaDB:', err);
        process.exit(1);
    }
}

function generateRandomString(length) {
    const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
    let result = '';
    for (let i = 0; i < length; i++) {
        result += chars.charAt(Math.floor(Math.random() * chars.length));
    }
    return result;
}

function generateRandomDate(start, end) {
    return new Date(start.getTime() + Math.random() * (end.getTime() - start.getTime()));
}

function generateRandomPrice(min, max) {
    return (Math.random() * (max - min) + min).toFixed(2);
}

function generateRandomInt(min, max) {
    return Math.floor(Math.random() * (max - min + 1)) + min;
}

async function generateCollections(conn, size) {
    const datasetConfig = config.datasets[size];
    
    try {
        const db = conn.getDB('test');
        
        try {
            db.dropCollection('customers');
            db.dropCollection('orders');
            db.dropCollection('products');
            db.dropCollection('order_items');
            db.dropCollection('suppliers');
            db.dropCollection('categories');
        } catch (err) {
        }
        
        db.createCollection('customers');
        db.createCollection('orders');
        db.createCollection('products');
        db.createCollection('order_items');
        db.createCollection('suppliers');
        db.createCollection('categories');
        
        db.getCollection('customers').createIndex({ customer_id: 1 }, { unique: true });
        db.getCollection('orders').createIndex({ order_id: 1 }, { unique: true });
        db.getCollection('orders').createIndex({ customer_id: 1 });
        db.getCollection('products').createIndex({ product_id: 1 }, { unique: true });
        db.getCollection('products').createIndex({ category_id: 1 });
        db.getCollection('products').createIndex({ supplier_id: 1 });
        db.getCollection('order_items').createIndex({ order_id: 1 });
        db.getCollection('order_items').createIndex({ product_id: 1 });
        db.getCollection('suppliers').createIndex({ supplier_id: 1 }, { unique: true });
        db.getCollection('categories').createIndex({ category_id: 1 }, { unique: true });
        
        await generateCategories(db, datasetConfig.categories);
        await generateSuppliers(db, datasetConfig.suppliers);
        await generateProducts(db, datasetConfig.products);
        await generateCustomers(db, datasetConfig.customers);
        await generateOrders(db, datasetConfig.orders);
        await generateOrderItems(db, datasetConfig.order_items);
        
        console.log('Data generation completed successfully.');
    } catch (err) {
        console.error('Error generating collections:', err);
        process.exit(1);
    }
}

async function generateCategories(db, count) {
    console.log(`Generating ${count} categories...`);
    const categories = db.getCollection('categories');
    
    for (let i = 1; i <= count; i++) {
        const category = {
            category_id: i,
            name: `Category ${i}`,
            description: `Description for category ${i}`
        };
        
        categories.insert(category);
        
        if (i % 100 === 0) {
            console.log(`Generated ${i}/${count} categories`);
        }
    }
}

async function generateSuppliers(db, count) {
    console.log(`Generating ${count} suppliers...`);
    const suppliers = db.getCollection('suppliers');
    
    for (let i = 1; i <= count; i++) {
        const supplier = {
            supplier_id: i,
            name: `Supplier ${i}`,
            contact_name: `Contact ${i}`,
            email: `supplier${i}@example.com`,
            phone: `555-${String(i).padStart(4, '0')}`,
            address: `${i} Supplier Street, Supplier City`
        };
        
        suppliers.insert(supplier);
        
        if (i % 100 === 0) {
            console.log(`Generated ${i}/${count} suppliers`);
        }
    }
}

async function generateProducts(db, count) {
    console.log(`Generating ${count} products...`);
    const products = db.getCollection('products');
    const categoryCount = config.datasets[size].categories;
    const supplierCount = config.datasets[size].suppliers;
    
    for (let i = 1; i <= count; i++) {
        const product = {
            product_id: i,
            name: `Product ${i}`,
            description: `Description for product ${i}`,
            price: generateRandomPrice(10, 1000),
            stock: generateRandomInt(0, 1000),
            category_id: generateRandomInt(1, categoryCount),
            supplier_id: generateRandomInt(1, supplierCount),
            created_at: generateRandomDate(new Date(2020, 0, 1), new Date())
        };
        
        products.insert(product);
        
        if (i % 1000 === 0) {
            console.log(`Generated ${i}/${count} products`);
        }
    }
}

async function generateCustomers(db, count) {
    console.log(`Generating ${count} customers...`);
    const customers = db.getCollection('customers');
    
    for (let i = 1; i <= count; i++) {
        const customer = {
            customer_id: i,
            first_name: `FirstName${i}`,
            last_name: `LastName${i}`,
            email: `customer${i}@example.com`,
            phone: `555-${String(i).padStart(4, '0')}`,
            address: `${i} Customer Street, Customer City`,
            created_at: generateRandomDate(new Date(2020, 0, 1), new Date())
        };
        
        customers.insert(customer);
        
        if (i % 1000 === 0) {
            console.log(`Generated ${i}/${count} customers`);
        }
    }
}

async function generateOrders(db, count) {
    console.log(`Generating ${count} orders...`);
    const orders = db.getCollection('orders');
    const customerCount = config.datasets[size].customers;
    
    const statuses = ['pending', 'processing', 'shipped', 'delivered', 'cancelled'];
    
    for (let i = 1; i <= count; i++) {
        const orderDate = generateRandomDate(new Date(2020, 0, 1), new Date());
        const deliveryDate = new Date(orderDate);
        deliveryDate.setDate(deliveryDate.getDate() + generateRandomInt(1, 14));
        
        const order = {
            order_id: i,
            customer_id: generateRandomInt(1, customerCount),
            order_date: orderDate,
            delivery_date: deliveryDate,
            status: statuses[generateRandomInt(0, statuses.length - 1)],
            total_amount: generateRandomPrice(50, 5000)
        };
        
        orders.insert(order);
        
        if (i % 1000 === 0) {
            console.log(`Generated ${i}/${count} orders`);
        }
    }
}

async function generateOrderItems(db, count) {
    console.log(`Generating ${count} order items...`);
    const orderItems = db.getCollection('order_items');
    const orderCount = config.datasets[size].orders;
    const productCount = config.datasets[size].products;
    
    for (let i = 1; i <= count; i++) {
        const orderId = generateRandomInt(1, orderCount);
        const productId = generateRandomInt(1, productCount);
        const quantity = generateRandomInt(1, 10);
        const price = generateRandomPrice(10, 1000);
        
        const orderItem = {
            order_item_id: i,
            order_id: orderId,
            product_id: productId,
            quantity: quantity,
            unit_price: price,
            total_price: (quantity * price).toFixed(2)
        };
        
        orderItems.insert(orderItem);
        
        if (i % 10000 === 0) {
            console.log(`Generated ${i}/${count} order items`);
        }
    }
}

async function main() {
    const conn = connectToSequoiaDB();
    await generateCollections(conn, size);
    conn.close();
}

main().catch(err => {
    console.error('Error in data generation:', err);
    process.exit(1);
});
